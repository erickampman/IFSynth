//
//  IFSOscModule.cpp — IFS fractal wavetable oscillator (IFSynth port of PM's IFSOscModule).
//
//  Differences from the AUv3 original:
//   - Trimmed to the simple v1 params (pitch/fine + the 5 fractal-shape controls + smoothing +
//     iterations). The whole modulation matrix (per-param mod inputs, phase mod) is deferred
//     until IFSynth has mod sources (LFO/Env) and a CV module, so the per-sample mod path is
//     gone — this is purely the wavetable + crossfade path.
//   - Band-limited mip pyramid (11 levels, FFT brick-wall truncation — see IFSCore) so high notes
//     don't alias. Like PM it lives in a cross-voice shared object (one pyramid per placed module,
//     not per voice), but PM's Apple-GCD background render is replaced by a SYNCHRONOUS render: the
//     first voice to see an uninitialised/changed shape renders it inline (rate-limited via a
//     cooldown ticked in beginBlock). No GCD, no threads, no RT-heap allocation.
//

#include "IFSOscModule.hpp"
#include "IFSCore.hpp"
#include "ModuleSharedState.hpp"
#include "PitchConverter.hpp"

#include <algorithm>
#include <cmath>

#include "pm_midi.h"

// Samples over which to crossfade when the wavetable changes.
static constexpr int IFS_CROSSFADE_SAMPLES = 256;

// Minimum samples between synchronous mip re-renders (~43 ms @ 48k). Rendering the pyramid is ~12×
// costlier than a single table (one IFS eval + 11 FFTs), so coalesce rapid shape-param automation:
// the LATEST shape is always what renders next, but at a bounded rate so a block never stalls (a
// host fuzzing params every block would otherwise re-render every block). A future shared/background
// render (see docs/AudioVerificationTests.md) would remove this compromise.
static constexpr int IFS_RENDER_COOLDOWN = 2048;

// Fast log2 approximation (~0.01 max error) — plenty accurate for mip-level selection, and far
// cheaper than std::log2f, which is called PER SAMPLE in mipRead (a real hot-loop cost: std::log2f
// there timed out clap-validator's param-fuzz on a large audio volume).
static inline float fastLog2(float x) {
    union { float f; uint32_t i; } vx = { x };
    union { uint32_t i; float f; } mx = { (vx.i & 0x007FFFFF) | 0x3F000000 };
    float y = (float)vx.i * 1.1920928955078125e-7f;   // vx.i * 2^-23
    return y - 124.22551499f - 1.498030302f * mx.f - 1.72587999f / (0.3520887068f + mx.f);
}

// ------------------------------------------------------------ State

// Cross-voice shared band-limited mip pyramid. The IFS waveform depends only on module-level shape
// params (identical across every voice copy), so the pyramid is rendered ONCE per placed module and
// shared — NOT 32× per voice. (Per-voice was 23 MB and 128 renders and timed out host param-fuzz;
// see docs/AudioVerificationTests.md.) Ported from PM's IFSOscSharedState but rendered SYNCHRONOUSLY
// (no GCD): the first voice to see an uninitialised/changed shape renders inline; beginBlock() ticks
// the coalescing cooldown. Voices process sequentially on the audio thread, so plain members are safe.
struct IFSOscSharedState : ModuleSharedState {
    float    mip[2][IFS_MIP_LEVELS][IFS_WAVETABLE_SIZE];
    int      activeIdx      = 0;   // voices read mip[activeIdx]
    uint64_t generation     = 0;   // ++ on each swap; drives each voice's crossfade
    int      renderCooldown = 0;   // samples until the next re-render is allowed (coalesces automation)
    bool     initialized    = false;
    IFSShapeParams activeShape;    // shape of the current active pyramid

    void reset() override {}
    void beginBlock(uint32_t /*activeVoices*/, uint32_t frameCount) override {
        renderCooldown -= (int)frameCount;   // once per chunk, before any voice processes
    }
};

ModuleSharedState* CreateIFSOscSharedState() { return new IFSOscSharedState(); }

static bool shapeChanged(const IFSShapeParams& a, const IFSShapeParams& b) {
    return a.ifs1Squash != b.ifs1Squash || a.ifs2Squash != b.ifs2Squash || a.bx != b.bx ||
           a.by != b.by || a.startEndY != b.startEndY || a.smoothing != b.smoothing ||
           a.iterations != b.iterations;
}

// Per-voice playback state (phase lives in Module; the pyramid is shared, above).
struct IFSOscState {
    int      crossfadePos   = 0;      // counts down from IFS_CROSSFADE_SAMPLES
    uint64_t seenGeneration = 0;      // last shared generation this voice crossfaded to
    bool     firstProcess   = true;   // skip the crossfade the first time this voice reads the pyramid
    float prevSyncL = 0.0f, prevSyncR = 0.0f; // last sync-input sample (zero-crossing detection)
    /// Pitch (octaves re A440) of the most recent MIDI note. Applied in the process loop only when
    /// the Pitch input is unconnected — a patched Pitch input overrides it.
    float midiPitch = 0.0f;
};

// Advance a phase by one sample; if hard sync is on and the sync input crosses zero (per mode),
// reset the phase sub-sample-accurately to the start of the IFS cycle. prevS is updated in place.
static inline void advanceSync(double& phase, double delta, bool syncOn, OscSyncType type,
                               float& prevS, float curS)
{
    phase += delta;
    // Wrap to [0,1) for ANY finite delta. A single `phase -= 1.0` cannot recover when the
    // per-sample delta exceeds 1.0 (extreme pitch, or a hot signal patched into the pitch/
    // pitch-mod input) — phase then runs away and later indexes the wavetable out of bounds.
    if (phase >= 1.0 || phase < 0.0) phase -= std::floor(phase);
    if (syncOn)
    {
        bool reset = false;
        switch (type)
        {
        case OscSyncTypeDown: reset = (prevS > 0.0f) && (curS <= 0.0f); break;
        case OscSyncTypeUp:   reset = (prevS < 0.0f) && (curS >= 0.0f); break;
        case OscSyncTypeBoth: reset = ((prevS > 0.0f) && (curS <= 0.0f))
                                   || ((prevS < 0.0f) && (curS >= 0.0f)); break;
        }
        if (reset) {
            const float t = prevS / (prevS - curS);
            phase = (1.0 - t) * delta;
            phase -= std::floor(phase);
        }
        prevS = curS;
    }
}

// Wavetable lookup with linear interpolation and phase wrapping. phase in [0, 1).
static inline float tableRead(const float* table, double phase)
{
    double pos  = phase * IFS_WAVETABLE_SIZE;
    int    idx  = static_cast<int>(pos) & (IFS_WAVETABLE_SIZE - 1);   // mask like idx1 — never index OOB
    float  frac = static_cast<float>(pos - std::floor(pos));
    int    idx1 = (idx + 1) & (IFS_WAVETABLE_SIZE - 1);
    return table[idx] + frac * (table[idx1] - table[idx]);
}

// Read the band-limited mip pyramid at `phase` for phase-increment `delta` (cycles/sample),
// selecting a level whose top harmonic stays below Nyquist so nothing folds. Lf = log2(N*delta) is
// the fractional minimum-safe level; we read the two levels just above floor(Lf) (both always safe)
// and crossfade by the fractional part — mixing floor..ceil would leak the unsafe sharp level.
static inline float mipRead(const float mip[IFS_MIP_LEVELS][IFS_WAVETABLE_SIZE],
                            double phase, double delta)
{
    constexpr int MAXL = IFS_MIP_LEVELS - 1;
    float arg  = static_cast<float>(IFS_WAVETABLE_SIZE * delta);
    float Lf   = arg > 0.0f ? fastLog2(arg) : 0.0f;
    Lf         = std::clamp(Lf, 0.0f, static_cast<float>(MAXL));
    int   base = static_cast<int>(std::floor(Lf));
    float frac = Lf - base;
    int   l0   = std::min(base + 1, MAXL);
    int   l1   = std::min(base + 2, MAXL);
    float a    = tableRead(mip[l0], phase);
    float b    = tableRead(mip[l1], phase);
    return a + frac * (b - a);
}

// ------------------------------------------------------------ Lifecycle
const void InitIFSOscModule(uint32_t /*count*/, Module* module)
{
    auto* st = new IFSOscState();
    // The mip pyramid lives in the shared state (rendered lazily on first process — module->sharedState
    // is not wired yet here, it's injected by Voice::initializeModules after this Init runs).

    // Pitch infrastructure — same pattern as SinOsc.
    double hz = 440.0;
    module->mBaseDeltaOmegaL = hz * module->mFrequencyScale;
    module->mBaseDeltaOmegaR = hz * module->mFrequencyScale;
    module->mOmegaL = 0.0;
    module->mOmegaR = 0.0;

    // The PitchConverter is shared: the voice assigns its global one (SinOsc does the same).
    // This module must not own or free it.
    module->state = st;
}

const void DeinitIFSOscModule(Module* module)
{
    auto* st = static_cast<IFSOscState*>(module->state);
    if (!st) return;
    // Do not touch module->pitchConverter — it's shared, owned by the voice, not this module.
    delete st;
    module->state = nullptr;
}

const void ResetIFSOscModule(Module* module)
{
    module->mOmegaL = 0.0;
    module->mOmegaR = 0.0;
    if (module->state) {
        auto* st = static_cast<IFSOscState*>(module->state);
        st->prevSyncL = st->prevSyncR = 0.0f;
        st->midiPitch = 0.0f;
    }
}

const void SampleRateIFSOscModule(Module* /*module*/, double /*newSampleRate*/)
{
    // mFrequencyScale is updated by Module::updateSampleRate() beforehand; the process loop
    // recomputes delta from pitch every sample, and the wavetable is SR-independent. No-op.
}

// ------------------------------------------------------------ Parameter change
// No-op: the shared pyramid detects shape changes by comparing the module's live shape params to the
// pyramid's activeShape each block (see ProcessIFSOscModule), so there's nothing to flag here.
const void ParamIFSOscModule(Module* /*module*/, AUParameterAddress /*address*/, AUValue /*value*/)
{
}

// ------------------------------------------------------------ MIDI
void MIDIIFSOscModule(const pm::UmpMessage& message, Module* module)
{
    // If pitch is driven by a connected input, note messages don't set frequency.
    if (module->isInputConnected(IFSOscParamItemPitchInput)) return;

    if (message.status == pm::kStatusNoteOn && message.velocity > 0) {
        double freq = 0.0;
        if (message.attributeType == pm::kAttrPitch7_9) {
            uint16_t attr     = message.attribute;
            uint8_t  note     = (attr & 0xFE00) >> 9;
            uint16_t fraction = attr & 0x01FF;
            if (fraction >= 512) fraction = 511;
            freq = pm::noteToFrequency(note) * pm::pitchFractionRatio(fraction);
        } else {
            freq = pm::noteToFrequency(message.note);
        }
        // Store the note as a pitch offset (octaves re A440); the process loop applies it only when
        // the Pitch input is unconnected. (Setting mBaseDeltaOmega is futile — the loop recomputes
        // delta from the pitch sum every sample.)
        static_cast<IFSOscState*>(module->state)->midiPitch =
            module->pitchConverter->frequencyToPitch(freq);
        // Retune above, but only reset phase for a fresh note — a legato continuation keeps its
        // phase so the slide is click-free. See Module::legatoContinuation's own comment.
        if (!module->legatoContinuation) {
            module->mOmegaL = 0.0;
            module->mOmegaR = 0.0;
        }
    }
}

// Read the module's base shape from its params (no modulation applied).
static IFSShapeParams baseShapeFromParams(Module* module)
{
    IFSShapeParams s;
    s.ifs1Squash = module->paramByEnum(IFSOscParamItemIFS1Squash);
    s.ifs2Squash = module->paramByEnum(IFSOscParamItemIFS2Squash);
    s.bx         = module->paramByEnum(IFSOscParamItemBx);
    s.by         = module->paramByEnum(IFSOscParamItemBy);
    s.startEndY  = module->paramByEnum(IFSOscParamItemStartEndY);
    s.smoothing  = module->paramByEnum(IFSOscParamItemSmoothing);
    s.iterations = static_cast<int>(module->paramByEnum(IFSOscParamItemIterations)) + 3;
    return s;
}

// ------------------------------------------------------------ Process
const KPBuffPair* ProcessIFSOscModule(uint64_t processCallID, uint32_t count, Module* module)
{
    auto* st = static_cast<IFSOscState*>(module->state);

    float* outL = module->output.first->data;
    float* outR = module->output.second->data;

    const AUValue freqParam = module->paramByEnum(IFSOscParamItemPitch)     * (float)SEMITONES_TO_PITCH;
    const AUValue fineParam = module->paramByEnum(IFSOscParamItemPitchFine) * (float)CENTS_TO_PITCH;
    // Stereo detune (cents → octaves), split symmetrically: L is flattened, R is sharpened by
    // half. A constant-cents split gives even width across the keyboard (beats speed up on high
    // notes — the classic detune behaviour).
    const float   detuneHalf = 0.5f * module->paramByEnum(IFSOscParamItemDetune) * (float)CENTS_TO_PITCH;
    const double  freqScale = module->mFrequencyScale;
    auto* pitchConverter    = module->pitchConverter;

    // Direct-MIDI pitch: the played note, applied only when nothing is patched to the Pitch input.
    const float midiPitch = module->isInputConnected(IFSOscParamItemPitchInput) ? 0.0f : st->midiPitch;

    double& phaseL = module->mOmegaL;
    double& phaseR = module->mOmegaR;
    double& deltaL = module->mBaseDeltaOmegaL;
    double& deltaR = module->mBaseDeltaOmegaR;

    auto pitchInputPair = module->retrieveInput(IFSOscParamItemPitchInput, processCallID, count);
    const float* pilPtr = pitchInputPair->first->data;
    const float* pirPtr = pitchInputPair->second->data;

    // Pitch mod (per-sample; wavetable-compatible).
    auto pitchModPair = module->retrieveInput(IFSOscParamItemPitchModInput, processCallID, count);
    const float* pmlPtr = pitchModPair->first->data;
    const float* pmrPtr = pitchModPair->second->data;
    const float pitchModLevel = module->paramByEnum(IFSOscParamItemPitchModLevel);

    // Second pitch input: the pitch bend, which arrives after the Slew so glide never smears it.
    auto pitchMod2Pair = module->retrieveInput(IFSOscParamItemPitchMod2Input, processCallID, count);
    const float* pm2lPtr = pitchMod2Pair->first->data;
    const float* pm2rPtr = pitchMod2Pair->second->data;
    const float pitchMod2Level = module->paramByEnum(IFSOscParamItemPitchMod2Level);

    // Hard sync (per-channel): the IFS osc is the slave; on a zero crossing of the sync input
    // (the sine master, stereo) the corresponding channel's phase resets, so the L/R detune of
    // the sine drives the L/R sync rate and the stereo width survives. Output mix level too.
    const int  syncMode = static_cast<int>(module->paramByEnum(IFSOscParamItemSyncMode));
    const bool syncOn   = syncMode >= 1 && module->isInputConnected(IFSOscParamItemSyncInput);
    const OscSyncType syncType = syncMode == 1 ? OscSyncTypeUp
                               : syncMode == 2 ? OscSyncTypeDown : OscSyncTypeBoth;
    auto syncPair = module->retrieveInput(IFSOscParamItemSyncInput, processCallID, count);
    const float* syncLPtr = syncPair->first->data;
    const float* syncRPtr = syncPair->second->data;
    const float  levelParam = module->paramByEnum(IFSOscParamItemLevel);

    // Shape mods (per-chunk; switch to the eval path if any is connected).
    const bool bxConn = module->isInputConnected(IFSOscParamItemBxModInput);
    const bool byConn = module->isInputConnected(IFSOscParamItemByModInput);
    const bool s1Conn = module->isInputConnected(IFSOscParamItemSquash1ModInput);
    const bool s2Conn = module->isInputConnected(IFSOscParamItemSquash2ModInput);
    const bool anyShapeMod = bxConn || byConn || s1Conn || s2Conn;

    if (!anyShapeMod) {
        // ---- Wavetable path (shared band-limited pyramid) ----
        auto* shared = static_cast<IFSOscSharedState*>(module->sharedState);
        const IFSShapeParams shape = baseShapeFromParams(module);
        // Render into the shared pyramid on first touch, or when the shape changed (rate-limited).
        // Voices run sequentially on the audio thread, so the first voice this block does the render
        // and the rest read the result. renderCooldown is ticked once per chunk in beginBlock().
        if (!shared->initialized) {
            computeIFSWavetableMips(shape, shared->mip[0]);
            std::copy_n(&shared->mip[0][0][0], IFS_MIP_LEVELS * IFS_WAVETABLE_SIZE, &shared->mip[1][0][0]);
            shared->activeShape  = shape;
            shared->initialized  = true;
        } else if (shapeChanged(shape, shared->activeShape) && shared->renderCooldown <= 0) {
            shared->renderCooldown = IFS_RENDER_COOLDOWN;   // coalesce rapid automation
            const int inactive = 1 - shared->activeIdx;
            computeIFSWavetableMips(shape, shared->mip[inactive]);
            shared->activeShape = shape;
            shared->activeIdx   = inactive;
            shared->generation++;
        }

        // Crossfade to the freshly-rendered pyramid when the shared generation advances. The first
        // time this voice reads the pyramid it just adopts the current generation (no crossfade).
        if (st->firstProcess) { st->seenGeneration = shared->generation; st->firstProcess = false; }
        else if (shared->generation != st->seenGeneration) {
            st->seenGeneration = shared->generation;
            st->crossfadePos   = IFS_CROSSFADE_SAMPLES;
        }
        const int activeIdx  = shared->activeIdx;
        const auto& activeMip = shared->mip[activeIdx];
        const auto& prevMip   = shared->mip[1 - activeIdx];
        bool crossfading      = st->crossfadePos > 0;

        for (uint32_t i = 0; i < count; ++i) {
            deltaL = pitchConverter->pitchToFrequency(pilPtr[i] + midiPitch + freqParam + fineParam - detuneHalf + pmlPtr[i] * pitchModLevel + pm2lPtr[i] * pitchMod2Level) * freqScale;
            deltaR = pitchConverter->pitchToFrequency(pirPtr[i] + midiPitch + freqParam + fineParam + detuneHalf + pmrPtr[i] * pitchModLevel + pm2rPtr[i] * pitchMod2Level) * freqScale;
            // Mip level tracks the phase increment (delta), so band-limiting follows pitch + pitch-mod.
            float sampleL = mipRead(activeMip, phaseL, deltaL);
            float sampleR = mipRead(activeMip, phaseR, deltaR);
            if (crossfading) {
                float blend = static_cast<float>(st->crossfadePos) / IFS_CROSSFADE_SAMPLES;
                sampleL += blend * (mipRead(prevMip, phaseL, deltaL) - sampleL);
                sampleR += blend * (mipRead(prevMip, phaseR, deltaR) - sampleR);
                --st->crossfadePos;
                crossfading = st->crossfadePos > 0;
            }
            outL[i] = sampleL * levelParam; outR[i] = sampleR * levelParam;
            advanceSync(phaseL, deltaL, syncOn, syncType, st->prevSyncL, syncLPtr[i]);
            advanceSync(phaseR, deltaR, syncOn, syncType, st->prevSyncR, syncRPtr[i]);
        }
        return &module->output;
    }

    // ---- Chunked modulated-eval path ----
    // Shape params are modulated at control rate (32-sample chunks): recompute the smoothing
    // vertices once per chunk, then evaluate smoothed per sample. Keeps `smoothing` working.
    const IFSShapeParams base = baseShapeFromParams(module);
    const float bxLevel = module->paramByEnum(IFSOscParamItemBxModLevel);
    const float byLevel = module->paramByEnum(IFSOscParamItemByModLevel);
    const float s1Level = module->paramByEnum(IFSOscParamItemSquash1ModLevel);
    const float s2Level = module->paramByEnum(IFSOscParamItemSquash2ModLevel);
    const float* bxM = module->retrieveInput(IFSOscParamItemBxModInput,      processCallID, count)->first->data;
    const float* byM = module->retrieveInput(IFSOscParamItemByModInput,      processCallID, count)->first->data;
    const float* s1M = module->retrieveInput(IFSOscParamItemSquash1ModInput, processCallID, count)->first->data;
    const float* s2M = module->retrieveInput(IFSOscParamItemSquash2ModInput, processCallID, count)->first->data;

    constexpr uint32_t CHUNK = 32;
    IFSVertices verts;
    for (uint32_t i0 = 0; i0 < count; i0 += CHUNK) {
        const uint32_t i1 = std::min(i0 + CHUNK, count);
        IFSShapeParams ms = base; // modulated shape for this chunk (sampled at its first frame)
        if (bxConn) ms.bx         = std::clamp(base.bx         + bxM[i0] * bxLevel, -0.98f, 0.98f);
        if (byConn) ms.by         = std::clamp(base.by         + byM[i0] * byLevel, -1.00f, 1.00f);
        if (s1Conn) ms.ifs1Squash = std::clamp(base.ifs1Squash + s1M[i0] * s1Level, -0.99f, 0.99f);
        if (s2Conn) ms.ifs2Squash = std::clamp(base.ifs2Squash + s2M[i0] * s2Level, -0.99f, 0.99f);
        precomputeIFSVertices(ms, verts);

        for (uint32_t j = i0; j < i1; ++j) {
            deltaL = pitchConverter->pitchToFrequency(pilPtr[j] + midiPitch + freqParam + fineParam - detuneHalf + pmlPtr[j] * pitchModLevel + pm2lPtr[j] * pitchMod2Level) * freqScale;
            deltaR = pitchConverter->pitchToFrequency(pirPtr[j] + midiPitch + freqParam + fineParam + detuneHalf + pmrPtr[j] * pitchModLevel + pm2rPtr[j] * pitchMod2Level) * freqScale;
            outL[j] = evaluateIFSSmoothed(static_cast<float>(phaseL), verts) * levelParam;
            outR[j] = evaluateIFSSmoothed(static_cast<float>(phaseR), verts) * levelParam;
            advanceSync(phaseL, deltaL, syncOn, syncType, st->prevSyncL, syncLPtr[j]);
            advanceSync(phaseR, deltaR, syncOn, syncType, st->prevSyncR, syncRPtr[j]);
        }
    }
    // No explicit dirty flag needed: when we next fall back to the wavetable path, it compares the
    // live shape to the shared pyramid's activeShape and re-renders if it changed.
    return &module->output;
}
