//
//  IFSOscModule.cpp — IFS fractal wavetable oscillator (IFSynth port of PM's IFSOscModule).
//
//  Differences from the AUv3 original:
//   - Trimmed to the simple v1 params (pitch/fine + the 5 fractal-shape controls + smoothing +
//     iterations). The whole modulation matrix (per-param mod inputs, phase mod) is deferred
//     until IFSynth has mod sources (LFO/Env) and a CV module, so the per-sample mod path is
//     gone — this is purely the wavetable + crossfade path.
//   - Apple GCD background render (dispatch_queue/source) is replaced by a single-threaded,
//     coalesced render at the top of the process block: a shape-param change flags the table
//     dirty; the next process() renders the inactive buffer once (~2048 samples, sub-0.1 ms),
//     swaps, and crossfades. No RT-heap allocation, no threads.
//

#include "IFSOscModule.hpp"
#include "IFSCore.hpp"
#include "PitchConverter.hpp"

#include <algorithm>
#include <cmath>

#include "pm_midi.h"

// Samples over which to crossfade when the wavetable changes.
static constexpr int IFS_CROSSFADE_SAMPLES = 256;

// ------------------------------------------------------------ State
struct IFSOscState {
    float wavetable[2][IFS_WAVETABLE_SIZE];
    int   activeIdx    = 0;   // process reads wavetable[activeIdx]
    bool  tableDirty   = true; // a shape param changed → re-render at next process
    int   crossfadePos = 0;    // counts down from IFS_CROSSFADE_SAMPLES

    IFSShapeParams pendingShape; // written by paramProc, consumed by the render
    IFSShapeParams activeShape;  // shape of the currently active table
};

// Snapshot the module's shape params into pendingShape.
static void applyShapeParams(IFSOscState* st, Module* module)
{
    IFSShapeParams& s = st->pendingShape;
    s.ifs1Squash = module->paramByEnum(IFSOscParamItemIFS1Squash);
    s.ifs2Squash = module->paramByEnum(IFSOscParamItemIFS2Squash);
    s.bx         = module->paramByEnum(IFSOscParamItemBx);
    s.by         = module->paramByEnum(IFSOscParamItemBy);
    s.startEndY  = module->paramByEnum(IFSOscParamItemStartEndY);
    s.smoothing  = module->paramByEnum(IFSOscParamItemSmoothing);
    // Parameter is 0-based (picker index): 0 → 3 iterations, 1 → 4 iterations.
    s.iterations = static_cast<int>(module->paramByEnum(IFSOscParamItemIterations)) + 3;
}

// Wavetable lookup with linear interpolation and phase wrapping. phase in [0, 1).
static inline float tableRead(const float* table, double phase)
{
    double pos  = phase * IFS_WAVETABLE_SIZE;
    int    idx  = static_cast<int>(pos);
    float  frac = static_cast<float>(pos - idx);
    int    idx1 = (idx + 1) & (IFS_WAVETABLE_SIZE - 1);
    return table[idx] + frac * (table[idx1] - table[idx]);
}

// ------------------------------------------------------------ Lifecycle
const void InitIFSOscModule(uint32_t /*count*/, Module* module)
{
    auto* st = new IFSOscState();

    // Render a valid default table now (params aren't set until addParams(), which runs after
    // this init). tableDirty forces the first process() to re-render from the real params.
    st->pendingShape = st->activeShape = IFSShapeParams{};
    computeIFSWavetable(st->activeShape, st->wavetable[0]);
    std::copy_n(st->wavetable[0], IFS_WAVETABLE_SIZE, st->wavetable[1]);
    st->activeIdx = 0;
    st->crossfadePos = 0;
    st->tableDirty = true;

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
}

const void SampleRateIFSOscModule(Module* /*module*/, double /*newSampleRate*/)
{
    // mFrequencyScale is updated by Module::updateSampleRate() beforehand; the process loop
    // recomputes delta from pitch every sample, and the wavetable is SR-independent. No-op.
}

// ------------------------------------------------------------ Parameter change
const void ParamIFSOscModule(Module* module, AUParameterAddress address, AUValue /*value*/)
{
    auto* st = static_cast<IFSOscState*>(module->state);
    if (!st) return;

    switch (MODULE_ITEM_FROM_ADDRESS(address)) {
        case IFSOscParamItemIFS1Squash:
        case IFSOscParamItemIFS2Squash:
        case IFSOscParamItemBx:
        case IFSOscParamItemBy:
        case IFSOscParamItemStartEndY:
        case IFSOscParamItemSmoothing:
        case IFSOscParamItemIterations:
            // Snapshot the new shape; the next process() renders it (coalesces rapid changes).
            applyShapeParams(st, module);
            st->tableDirty = true;
            break;
        default:
            // Pitch / fine — used directly in the process loop.
            break;
    }
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
        module->mBaseDeltaOmegaL = freq * module->mFrequencyScale;
        module->mBaseDeltaOmegaR = freq * module->mFrequencyScale;
        module->mOmegaL = 0.0;
        module->mOmegaR = 0.0;
    }
}

// ------------------------------------------------------------ Process
const KPBuffPair* ProcessIFSOscModule(uint64_t processCallID, uint32_t count, Module* module)
{
    auto* st = static_cast<IFSOscState*>(module->state);

    float* outL = module->output.first->data;
    float* outR = module->output.second->data;

    // Coalesced wavetable render: at most one per block, on the audio thread (cheap + bounded).
    if (st->tableDirty) {
        st->tableDirty = false;
        applyShapeParams(st, module);
        const int inactive = 1 - st->activeIdx;
        computeIFSWavetable(st->pendingShape, st->wavetable[inactive]);
        st->activeIdx   = inactive;
        st->activeShape = st->pendingShape;
        st->crossfadePos = IFS_CROSSFADE_SAMPLES;
    }

    auto pitchInputPair = module->retrieveInput(IFSOscParamItemPitchInput, processCallID, count);

    const AUValue freqParam = module->paramByEnum(IFSOscParamItemPitch)     * (float)SEMITONES_TO_PITCH;
    const AUValue fineParam = module->paramByEnum(IFSOscParamItemPitchFine) * (float)CENTS_TO_PITCH;

    const double freqScale = module->mFrequencyScale;
    auto* pitchConverter   = module->pitchConverter;

    double& phaseL = module->mOmegaL;
    double& phaseR = module->mOmegaR;
    double& deltaL = module->mBaseDeltaOmegaL;
    double& deltaR = module->mBaseDeltaOmegaR;

    const float* pilPtr = pitchInputPair->first->data;
    const float* pirPtr = pitchInputPair->second->data;

    const int    activeIdx = st->activeIdx;
    const float* activeTbl = st->wavetable[activeIdx];
    const float* prevTbl   = st->wavetable[1 - activeIdx];
    bool crossfading       = st->crossfadePos > 0;

    for (uint32_t i = 0; i < count; ++i) {
        deltaL = pitchConverter->pitchToFrequency(pilPtr[i] + freqParam + fineParam) * freqScale;
        deltaR = pitchConverter->pitchToFrequency(pirPtr[i] + freqParam + fineParam) * freqScale;

        float sampleL = tableRead(activeTbl, phaseL);
        float sampleR = tableRead(activeTbl, phaseR);

        if (crossfading) {
            float blend = static_cast<float>(st->crossfadePos) / IFS_CROSSFADE_SAMPLES;
            sampleL += blend * (tableRead(prevTbl, phaseL) - sampleL);
            sampleR += blend * (tableRead(prevTbl, phaseR) - sampleR);
            --st->crossfadePos;
            crossfading = st->crossfadePos > 0;
        }

        outL[i] = sampleL;
        outR[i] = sampleR;

        phaseL += deltaL; if (phaseL >= 1.0) phaseL -= 1.0;
        phaseR += deltaR; if (phaseR >= 1.0) phaseR -= 1.0;
    }

    return &module->output;
}
