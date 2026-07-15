//
//  IFSLFOModule.cpp — IFS fractal LFO (IFSynth port of PM's IFSLFOModule).
//
//  Free-running: a phase advances at rate = 2^Pitch Hz (Pitch in octaves, 0 = 1 Hz) through a
//  2-transform IFS wavetable, output bipolar [-1,1]. Trimmed to the simple v1 params; the delta
//  is constant per block (no FM input), so no Accelerate is needed. Same coalesced render as
//  IFSOsc. Reuses IFSCore::computeIFSWavetable + the IFSShapeParams shape.
//

#include "IFSLFOModule.hpp"
#include "IFSCore.hpp"

#include <algorithm>
#include <cmath>

static constexpr int IFS_LFO_CROSSFADE_SAMPLES = 256;

struct IFSLFOState {
    float wavetable[2][IFS_WAVETABLE_SIZE];
    int   activeIdx    = 0;
    bool  tableDirty   = true;
    int   crossfadePos = 0;
    IFSShapeParams pendingShape;
    IFSShapeParams activeShape;
};

static void applyShapeParams(IFSLFOState* st, Module* module)
{
    IFSShapeParams& s = st->pendingShape;
    s.ifs1Squash = module->paramByEnum(IFSLFOParamItemIFS1Squash);
    s.ifs2Squash = module->paramByEnum(IFSLFOParamItemIFS2Squash);
    s.bx         = module->paramByEnum(IFSLFOParamItemBx);
    s.by         = module->paramByEnum(IFSLFOParamItemBy);
    s.startEndY  = module->paramByEnum(IFSLFOParamItemStartEndY);
    s.smoothing  = module->paramByEnum(IFSLFOParamItemSmoothing);
    s.iterations = static_cast<int>(module->paramByEnum(IFSLFOParamItemIterations)) + 1;
}

static inline float tableRead(const float* table, double phase)
{
    double pos  = phase * IFS_WAVETABLE_SIZE;
    int    idx  = static_cast<int>(pos);
    float  frac = static_cast<float>(pos - idx);
    int    idx1 = (idx + 1) & (IFS_WAVETABLE_SIZE - 1);
    return table[idx] + frac * (table[idx1] - table[idx]);
}

const void InitIFSLFOModule(uint32_t /*count*/, Module* module)
{
    auto* st = new IFSLFOState();
    st->activeShape = st->pendingShape = IFSShapeParams{};
    computeIFSWavetable(st->activeShape, st->wavetable[0]);
    std::copy_n(st->wavetable[0], IFS_WAVETABLE_SIZE, st->wavetable[1]);
    st->tableDirty = true;
    module->mOmegaL = 0.0;
    module->mOmegaR = 0.0;
    module->state = st;
}

const void DeinitIFSLFOModule(Module* module)
{
    delete static_cast<IFSLFOState*>(module->state);
    module->state = nullptr;
}

const void ParamIFSLFOModule(Module* module, AUParameterAddress address, AUValue /*value*/)
{
    auto* st = static_cast<IFSLFOState*>(module->state);
    if (!st) return;
    switch (MODULE_ITEM_FROM_ADDRESS(address)) {
        case IFSLFOParamItemBx:
        case IFSLFOParamItemBy:
        case IFSLFOParamItemIFS1Squash:
        case IFSLFOParamItemIFS2Squash:
        case IFSLFOParamItemStartEndY:
        case IFSLFOParamItemSmoothing:
        case IFSLFOParamItemIterations:
            applyShapeParams(st, module);
            st->tableDirty = true;
            break;
        default: // Pitch — used directly in process
            break;
    }
}

const KPBuffPair* ProcessIFSLFOModule(uint64_t /*processCallID*/, uint32_t count, Module* module)
{
    auto* st = static_cast<IFSLFOState*>(module->state);

    float* outL = module->output.first->data;
    float* outR = module->output.second->data;

    if (st->tableDirty) {
        st->tableDirty = false;
        applyShapeParams(st, module);
        const int inactive = 1 - st->activeIdx;
        computeIFSWavetable(st->pendingShape, st->wavetable[inactive]);
        st->activeIdx    = inactive;
        st->activeShape  = st->pendingShape;
        st->crossfadePos = IFS_LFO_CROSSFADE_SAMPLES;
    }

    // Rate: 2^Pitch Hz (Pitch in octaves, 0 → 1 Hz). Constant per block.
    const double delta = std::exp2(static_cast<double>(module->paramByEnum(IFSLFOParamItemPitch)))
                         * module->mFrequencyScale;

    double& phase = module->mOmegaL;

    const int    activeIdx = st->activeIdx;
    const float* activeTbl = st->wavetable[activeIdx];
    const float* prevTbl   = st->wavetable[1 - activeIdx];
    bool crossfading       = st->crossfadePos > 0;

    for (uint32_t i = 0; i < count; ++i) {
        float sample = tableRead(activeTbl, phase);
        if (crossfading) {
            const float blend = static_cast<float>(st->crossfadePos) / IFS_LFO_CROSSFADE_SAMPLES;
            sample += blend * (tableRead(prevTbl, phase) - sample);
            --st->crossfadePos;
            crossfading = st->crossfadePos > 0;
        }
        outL[i] = sample;
        outR[i] = sample;
        phase += delta;
        if (phase >= 1.0) phase -= 1.0;
    }

    module->mOmegaR = phase;
    return &module->output;
}
