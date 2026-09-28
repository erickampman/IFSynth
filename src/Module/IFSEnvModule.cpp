//
//  IFSEnvModule.cpp — IFS fractal envelope (IFSynth port of PM's IFSEnvModule).
//
//  The envelope advances a phase through a 3-transform IFS wavetable over `Duration` seconds,
//  triggered by MIDI note-on/off (or a connected gate). Hold modes freeze at the Blue/Green
//  point while the note is held (giving ADSR-like sustain); note-off resumes to the end.
//  Same portability changes as IFSOsc: Apple GCD replaced by a coalesced synchronous render at
//  the top of process() (dirty flag → render inactive buffer, swap, crossfade). No threads.
//

#include "IFSEnvModule.hpp"
#include "IFSCore.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "pm_midi.h"

static constexpr int   IFS_ENV_CROSSFADE_SAMPLES = 256;
static constexpr float IFS_ENV_STEAL_SLIDE_SECS  = 0.005f; // ~5 ms de-click on note steal

// ------------------------------------------------------------ State
struct IFSEnvState {
    float wavetable[2][IFS_WAVETABLE_SIZE];
    int   activeIdx    = 0;
    bool  tableDirty   = true; // a shape param changed → re-render at next process
    int   crossfadePos = 0;
    // The very first real render replaces the meaningless init-default table (startEndY=-1, peak 1.0)
    // that Init() seeds into BOTH table slots. Because a voice is inactive — and so not processed —
    // until its first note, that first render is deferred to note onset; crossfading into it from the
    // default table injects a large transient right as the oscillator swings (a click at the start).
    // So the first render swaps instantly (no crossfade); only later shape changes crossfade.
    bool  firstTableRender = true;

    IFSEnvShapeParams pendingShape;
    IFSEnvShapeParams activeShape;

    // playback state (audio thread only)
    double phase          = 0.0;   // position in [0, IFS_WAVETABLE_SIZE)
    double phasePerSample = 0.0;
    bool   active         = false;
    bool   heldAtHold     = false;
    float  holdIndex      = -1.0f; // wavetable position to freeze at (< 0 = none)
    bool   loopOnEnd      = false; // holdMode 3
    bool   noteHeld       = false;

    float  lastOutput     = -1.0f;

    int    stealRemaining    = 0;
    float  stealFrom         = 0.0f;
    int    stealSlideSamples = 221;

    float  prevGateValue = 0.0f;

    // "Scale Release" (IFSEnvParamItemScaleRelease): set when an early note-off (before reaching the
    // hold point, hold modes 1/2 only) jumps phase to holdIndex and plays the release tail remapped
    // to start at the level the envelope was at when released, still landing at startEndY. See
    // triggerNoteOff()/ProcessIFSEnvModule(). Cleared by a new note-on, the tail completing, reset.
    bool   scaledRelease   = false;
    bool   releaseFlat     = false;   // degenerate V_hold==V_end path — linear crossfade instead of affine
    float  releaseScale    = 1.0f;    // affine factor: out = startEndY + (raw - startEndY) * releaseScale
    float  releaseFromLevel = 0.0f;   // flat-path start level (V_current at release)
};

// ------------------------------------------------------------ Helpers
static float computeHoldIndex(const IFSEnvShapeParams& shape)
{
    switch (shape.holdMode) {
        case 1: return std::clamp(shape.blueX,  0.0f, 1.0f) * static_cast<float>(IFS_WAVETABLE_SIZE);
        case 2: return std::clamp(shape.greenX, 0.0f, 1.0f) * static_cast<float>(IFS_WAVETABLE_SIZE);
        default: return -1.0f;
    }
}

static void applyShapeParams(IFSEnvState* st, Module* module)
{
    IFSEnvShapeParams& s = st->pendingShape;
    s.blueX      = module->paramByEnum(IFSEnvParamItemBlueX);
    s.blueY      = module->paramByEnum(IFSEnvParamItemBlueY);
    s.greenX     = module->paramByEnum(IFSEnvParamItemGreenX);
    s.greenY     = module->paramByEnum(IFSEnvParamItemGreenY);
    s.startEndY  = module->paramByEnum(IFSEnvParamItemStartEndY);
    s.ifs1Squash = module->paramByEnum(IFSEnvParamItemIFS1Squash);
    s.ifs2Squash = module->paramByEnum(IFSEnvParamItemIFS2Squash);
    s.ifs3Squash = module->paramByEnum(IFSEnvParamItemIFS3Squash);
    s.smoothing  = module->paramByEnum(IFSEnvParamItemSmoothing);
    // Picker is 0-based: 0 → 1 iteration.
    s.iterations = static_cast<int>(module->paramByEnum(IFSEnvParamItemIterations)) + 1;
    s.holdMode   = static_cast<int>(module->paramByEnum(IFSEnvParamItemHoldMode));
}

static void updatePhaseRate(IFSEnvState* st, Module* module)
{
    float duration = module->paramByEnum(IFSEnvParamItemDuration);
    if (duration < 0.001f) duration = 0.001f;
    st->phasePerSample = static_cast<double>(IFS_WAVETABLE_SIZE) * module->mFrequencyScale
                         / static_cast<double>(duration);
}

static void updateStealSlideSamples(IFSEnvState* st, Module* module)
{
    const double sr = 1.0 / module->mFrequencyScale;
    st->stealSlideSamples = std::max(1, static_cast<int>(IFS_ENV_STEAL_SLIDE_SECS * sr));
}

static inline float envTableRead(const float* table, double phase)
{
    int   idx  = static_cast<int>(phase);
    float frac = static_cast<float>(phase - idx);
    int   idx1 = (idx + 1 < IFS_WAVETABLE_SIZE) ? idx + 1 : IFS_WAVETABLE_SIZE - 1;
    return table[idx] + frac * (table[idx1] - table[idx]);
}

// Legato mode 1 (FindMatchingY): closest phase to `target` in the active table, searching only
// [0, maxIdx]. When a hold point exists (modes 1/2), the caller passes maxIdx = holdIndex so the
// match is confined to the ATTACK region: a level like 0.28 typically occurs twice (once rising,
// once on the release tail), and matching the release-tail copy would put phase PAST holdIndex,
// whereupon the very next sample's hold check snaps phase back to holdIndex — a large output jump
// (the "volume increase" click). Confining to the attack side makes the envelope resume rising
// toward the hold point smoothly. Without a hold point the caller passes the full table length.
static double findMatchingPhase(const float* table, float target, int maxIdx)
{
    maxIdx = std::clamp(maxIdx, 0, IFS_WAVETABLE_SIZE - 1);
    int   bestIdx  = 0;
    float bestDiff = std::fabs(table[0] - target);
    for (int i = 1; i <= maxIdx; ++i) {
        float diff = std::fabs(table[i] - target);
        if (diff < bestDiff) { bestDiff = diff; bestIdx = i; }
    }
    return static_cast<double>(bestIdx);
}

static void triggerNoteOn(IFSEnvState* st, Module* module)
{
    st->noteHeld = true;
    st->scaledRelease = false;   // a fresh note cancels any in-progress scaled release
    if (st->active || st->heldAtHold || st->stealRemaining > 0) {
        const int legatoMode = static_cast<int>(module->paramByEnum(IFSEnvParamItemLegatoMode));
        if (legatoMode == 1) {
            // Confine the level match to the attack region when a hold point exists, so the
            // envelope resumes rising toward hold instead of matching on the release tail and then
            // snapping back to holdIndex on the next sample (see findMatchingPhase()).
            const int searchLimit = (st->holdIndex >= 0.0f)
                ? static_cast<int>(std::ceil(st->holdIndex))
                : (IFS_WAVETABLE_SIZE - 1);
            st->phase          = findMatchingPhase(st->wavetable[st->activeIdx], st->lastOutput, searchLimit);
            st->active         = true;
            st->heldAtHold     = false;
            st->stealRemaining = 0;
        } else {
            st->stealFrom      = st->lastOutput;
            st->stealRemaining = st->stealSlideSamples;
            st->active         = false;
            st->heldAtHold     = false;
        }
    } else {
        st->phase      = 0.0;
        st->active     = true;
        st->heldAtHold = false;
    }
}

static void triggerNoteOff(IFSEnvState* st, Module* module)
{
    st->noteHeld = false;
    if (st->heldAtHold) {
        // Released exactly at the hold point: resume the tail normally (current level == the hold
        // value here, so scaling would be a no-op anyway — nothing special to do).
        st->heldAtHold = false;
        st->active     = true;
        return;
    }
    // "Scale Release": an early release (still advancing, before the hold point) jumps to the hold
    // point and plays the release tail remapped to start at the current level. Only meaningful when
    // there IS a hold point (modes 1/2 — holdIndex >= 0); a no-op otherwise, matching today's
    // "run the full cycle" behavior when the param is off.
    if (module->paramByEnum(IFSEnvParamItemScaleRelease) >= 0.5f
        && st->active && st->holdIndex >= 0.0f
        && st->phase < static_cast<double>(st->holdIndex)) {
        const float vCurrent = st->lastOutput;
        const float vHold    = envTableRead(st->wavetable[st->activeIdx], static_cast<double>(st->holdIndex));
        const float vEnd     = module->paramByEnum(IFSEnvParamItemStartEndY);
        if (std::fabs(vHold - vEnd) > 1e-6f) {
            // Affine remap pinning the end: raw hold value -> vCurrent, raw end value (== vEnd) -> vEnd.
            st->releaseScale = (vCurrent - vEnd) / (vHold - vEnd);
            st->releaseFlat  = false;
        } else {
            // Degenerate: hold value == end value, so no affine map can both start at vCurrent and
            // end at vEnd. Fall back to a straight level crossfade across the tail by phase.
            st->releaseFromLevel = vCurrent;
            st->releaseFlat      = true;
        }
        st->scaledRelease = true;
        st->phase         = static_cast<double>(st->holdIndex);
        st->active        = true;
    }
}

// ------------------------------------------------------------ Lifecycle
const void InitIFSEnvModule(uint32_t /*count*/, Module* module)
{
    auto* st = new IFSEnvState();

    // Render a valid default table (params aren't set until addParams(), after this init);
    // tableDirty forces the first process() to re-render from the real params.
    st->activeShape = st->pendingShape = IFSEnvShapeParams{};
    st->holdIndex   = computeHoldIndex(st->activeShape);
    st->loopOnEnd   = (st->activeShape.holdMode == 3);
    st->lastOutput  = st->activeShape.startEndY;
    computeIFSEnvWavetable(st->activeShape, st->wavetable[0]);
    std::copy_n(st->wavetable[0], IFS_WAVETABLE_SIZE, st->wavetable[1]);
    st->tableDirty = true;

    static constexpr float kDefaultDurationSecs = 1.0f;
    st->phasePerSample = static_cast<double>(IFS_WAVETABLE_SIZE) * module->mFrequencyScale
                         / static_cast<double>(kDefaultDurationSecs);
    updateStealSlideSamples(st, module);

    module->state = st;
}

const void DeinitIFSEnvModule(Module* module)
{
    delete static_cast<IFSEnvState*>(module->state);
    module->state = nullptr;
}

const void ResetIFSEnvModule(Module* module)
{
    auto* st = static_cast<IFSEnvState*>(module->state);
    if (!st) return;
    st->phase = 0.0;
    st->active = false;
    st->heldAtHold = false;
    st->noteHeld = false;
    st->stealRemaining = 0;
    st->prevGateValue = 0.0f;
    st->scaledRelease = false;
    st->lastOutput = module->paramByEnum(IFSEnvParamItemStartEndY);
}

const void SampleRateIFSEnvModule(Module* module, double /*newSampleRate*/)
{
    auto* st = static_cast<IFSEnvState*>(module->state);
    if (!st) return;
    updatePhaseRate(st, module);
    updateStealSlideSamples(st, module);
}

// ------------------------------------------------------------ Parameter change
const void ParamIFSEnvModule(Module* module, AUParameterAddress address, AUValue /*value*/)
{
    auto* st = static_cast<IFSEnvState*>(module->state);
    if (!st) return;

    switch (MODULE_ITEM_FROM_ADDRESS(address)) {
        case IFSEnvParamItemBlueX:
        case IFSEnvParamItemBlueY:
        case IFSEnvParamItemGreenX:
        case IFSEnvParamItemGreenY:
        case IFSEnvParamItemStartEndY:
        case IFSEnvParamItemIFS1Squash:
        case IFSEnvParamItemIFS2Squash:
        case IFSEnvParamItemIFS3Squash:
        case IFSEnvParamItemSmoothing:
        case IFSEnvParamItemIterations:
        case IFSEnvParamItemHoldMode:
            applyShapeParams(st, module);
            st->tableDirty = true;
            break;
        case IFSEnvParamItemDuration:
            updatePhaseRate(st, module);
            break;
        default:
            // GateInput / LegatoMode — read live where needed.
            break;
    }
}

// ------------------------------------------------------------ MIDI
void MIDIIFSEnvModule(const pm::UmpMessage& message, Module* module)
{
    if (module->isInputConnected(IFSEnvParamItemGateInput)) return; // gate takes precedence
    auto* st = static_cast<IFSEnvState*>(module->state);
    if (!st) return;
    if (pm::isNoteOn(message))       triggerNoteOn(st, module);
    else if (pm::isNoteOff(message)) triggerNoteOff(st, module);
}

// ------------------------------------------------------------ Process
const KPBuffPair* ProcessIFSEnvModule(uint64_t processCallID, uint32_t count, Module* module)
{
    auto* st = static_cast<IFSEnvState*>(module->state);

    float* outL = module->output.first->data;
    float* outR = module->output.second->data;

    // Coalesced wavetable render (once per block, on the audio thread; ~2048 samples).
    if (st->tableDirty) {
        st->tableDirty = false;
        applyShapeParams(st, module);
        const int inactive = 1 - st->activeIdx;
        computeIFSEnvWavetable(st->pendingShape, st->wavetable[inactive]);
        st->activeIdx    = inactive;
        st->activeShape  = st->pendingShape;
        st->holdIndex    = computeHoldIndex(st->activeShape);
        st->loopOnEnd    = (st->activeShape.holdMode == 3);
        // Skip the crossfade on the first real render (init default → real params); see
        // firstTableRender's comment. Subsequent shape changes crossfade to de-click the swap.
        st->crossfadePos   = st->firstTableRender ? 0 : IFS_ENV_CROSSFADE_SAMPLES;
        st->firstTableRender = false;
        updatePhaseRate(st, module);
    }

    const int    activeIdx = st->activeIdx;
    const float* activeTbl = st->wavetable[activeIdx];
    const float* prevTbl   = st->wavetable[1 - activeIdx];
    bool crossfading       = st->crossfadePos > 0;

    const float startEndY = module->paramByEnum(IFSEnvParamItemStartEndY);
    const float holdIndex = st->holdIndex;

    const bool   hasGate = module->isInputConnected(IFSEnvParamItemGateInput);
    const float* gate    = nullptr;
    if (hasGate) {
        auto gatePair = module->retrieveInput(IFSEnvParamItemGateInput, processCallID, count);
        gate = gatePair->first->data;
    }

    for (uint32_t i = 0; i < count; ++i) {
        if (hasGate) {
            const float cur = gate[i], prev = st->prevGateValue;
            if (prev < 0.5f && cur >= 0.5f) triggerNoteOn(st, module);
            else if (prev >= 0.5f && cur < 0.5f) triggerNoteOff(st, module);
            st->prevGateValue = cur;
        }

        float out;
        if (st->stealRemaining > 0) {
            const float t = std::clamp(static_cast<float>(st->stealRemaining)
                                       / static_cast<float>(st->stealSlideSamples), 0.0f, 1.0f);
            out = st->stealFrom + (startEndY - st->stealFrom) * (1.0f - t);
            if (--st->stealRemaining == 0) { st->phase = 0.0; st->active = true; st->heldAtHold = false; }
        } else if (st->active) {
            if (st->phase >= static_cast<double>(IFS_WAVETABLE_SIZE)) {
                const bool gateActive = hasGate ? (gate[i] >= 0.5f) : st->noteHeld;
                if (st->loopOnEnd && gateActive) {
                    st->phase = 0.0;
                    out = envTableRead(activeTbl, st->phase);
                    st->phase += st->phasePerSample;
                } else {
                    st->active = false;
                    st->scaledRelease = false;   // tail finished — see triggerNoteOff()
                    out = startEndY;             // pinned end (scaled or not, it lands here)
                }
            } else {
                out = envTableRead(activeTbl, st->phase);
                if (crossfading) {
                    const float blend = static_cast<float>(st->crossfadePos) / IFS_ENV_CROSSFADE_SAMPLES;
                    out += blend * (envTableRead(prevTbl, st->phase) - out);
                }
                // "Scale Release": remap the tail so it began at the level the envelope was at when
                // released, while still ending at startEndY (see triggerNoteOff()). Applied to the
                // raw table value before the phase advances.
                if (st->scaledRelease) {
                    if (!st->releaseFlat) {
                        out = startEndY + (out - startEndY) * st->releaseScale;
                    } else {
                        const double span = static_cast<double>(IFS_WAVETABLE_SIZE) - holdIndex;
                        float prog = span > 0.0 ? static_cast<float>((st->phase - holdIndex) / span) : 1.0f;
                        prog = std::clamp(prog, 0.0f, 1.0f);
                        out = st->releaseFromLevel + (startEndY - st->releaseFromLevel) * prog;
                    }
                }
                st->phase += st->phasePerSample;
                if (holdIndex >= 0.0f && st->phase >= static_cast<double>(holdIndex)) {
                    const bool gateActive = hasGate ? (gate[i] >= 0.5f) : st->noteHeld;
                    if (gateActive) { st->phase = static_cast<double>(holdIndex); st->active = false; st->heldAtHold = true; }
                }
            }
        } else if (st->heldAtHold) {
            out = envTableRead(activeTbl, static_cast<double>(holdIndex));
            if (crossfading) {
                const float blend = static_cast<float>(st->crossfadePos) / IFS_ENV_CROSSFADE_SAMPLES;
                out += blend * (envTableRead(prevTbl, static_cast<double>(holdIndex)) - out);
            }
        } else {
            out = startEndY;
        }

        if (crossfading) { --st->crossfadePos; crossfading = st->crossfadePos > 0; }

        st->lastOutput = out;
        outL[i] = out;
        outR[i] = out;
    }

    return &module->output;
}
