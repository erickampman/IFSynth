// LevelModule.cpp — output = input × level (stereo). See LevelModule.hpp.

#include "LevelModule.hpp"
#include "pm_params.h"

const KPBuffPair* ProcessLevelModule(uint64_t processCallID, uint32_t count, Module* module)
{
    if (!module->isInputConnected(LevelParamItemInput))
        return KPBuffPairHelper::zeroes();

    auto in = module->retrieveInput(LevelParamItemInput, processCallID, count);
    const float level = module->paramByEnum(LevelParamItemLevel);

    const float* iL = in->first->data;
    const float* iR = in->second->data;
    float* oL = module->output.first->data;
    float* oR = module->output.second->data;

    for (uint32_t i = 0; i < count; ++i) {
        oL[i] = iL[i] * level;
        oR[i] = iR[i] * level;
    }
    return &module->output;
}
