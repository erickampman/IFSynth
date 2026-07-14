#include "pm_param_data.h"

// SinOsc — 11 params. All defaults 0 (→ produces A440 with no pitch input connected).
// name / item / unit / flags / valueStrings / min / max / def / info
static const ModuleParamConstants sinOscConstants[] = {
   {"pitch",         SinOscParamItemPitch,         0, 0, nullptr, -24.0f, 24.0f, 0.0f, 0},
   {"cents",         SinOscParamItemPitchFine,     0, 0, nullptr, -100.0f, 100.0f, 0.0f, 0},
   {"pitchInput",    SinOscParamItemPitchInput,    0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"modPitch",      SinOscParamItemModPitch,      0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"modPitch2",     SinOscParamItemModPitch2,     0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"modPhase",      SinOscParamItemModPhase,      0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"freqModLevel",  SinOscParamItemFreqModLevel,  0, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"phaseModLevel", SinOscParamItemPhaseModLevel, 0, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"freqModLevel2", SinOscParamItemFreqModLevel2, 0, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
   {"modSync",       SinOscParamItemModSync,       0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"modSyncType",   SinOscParamItemModSyncType,   0, 0, nullptr, 0.0f, 2.0f, 0.0f, 0},
};

const ModuleParamConstantsGroup sinOscConstantsGroup = {
   const_cast<char*>("sinOsc"),
   sizeof(sinOscConstants) / sizeof(sinOscConstants[0]),
   sinOscConstants,
};

// IFS oscillator — trimmed v1. Defaults match IFSShapeParams{} in IFSCore.hpp so the initial
// wavetable and the parameter tree agree.
static const ModuleParamConstants ifsOscConstants[] = {
   {"pitchInput", IFSOscParamItemPitchInput, 0, 0, nullptr,   0.0f,   0.0f,  0.0f, 0},
   {"pitch",      IFSOscParamItemPitch,      0, 0, nullptr, -24.0f,  24.0f,  0.0f, 0},
   {"cents",      IFSOscParamItemPitchFine,  0, 0, nullptr, -100.0f, 100.0f, 0.0f, 0},
   {"ifs1Squash", IFSOscParamItemIFS1Squash, 0, 0, nullptr, -0.99f,  0.99f,  0.5f, 0},
   {"ifs2Squash", IFSOscParamItemIFS2Squash, 0, 0, nullptr, -0.99f,  0.99f, -0.5f, 0},
   {"bx",         IFSOscParamItemBx,         0, 0, nullptr,  -1.0f,   1.0f,  0.0f, 0},
   {"by",         IFSOscParamItemBy,         0, 0, nullptr,  -1.0f,   1.0f,  0.5f, 0},
   {"startEndY",  IFSOscParamItemStartEndY,  0, 0, nullptr,  -1.0f,   1.0f, -1.0f, 0},
   {"smoothing",  IFSOscParamItemSmoothing,  0, 0, nullptr,   0.0f,   1.0f,  0.5f, 0},
   {"iterations", IFSOscParamItemIterations, 0, 0, nullptr,   0.0f,   1.0f,  1.0f, 0},
};
const ModuleParamConstantsGroup ifsOscConstantsGroup = {
   const_cast<char*>("ifsOsc"),
   sizeof(ifsOscConstants) / sizeof(ifsOscConstants[0]),
   ifsOscConstants,
};

// ADSR — 7 params. Times in seconds; sustain is a level 0..1.
static const ModuleParamConstants adsrConstants[] = {
   {"attack",    ADSRParamItemAttack,    0, 0, nullptr, 0.001f, 10.0f, 0.01f, 0},
   {"decay",     ADSRParamItemDecay,     0, 0, nullptr, 0.001f, 10.0f, 1.00f, 0},
   {"sustain",   ADSRParamItemSustain,   0, 0, nullptr, 0.0f, 1.0f, 0.5f, 0},
   {"release",   ADSRParamItemRelease,   0, 0, nullptr, 0.001f, 10.0f, 1.00f, 0},
   {"gateInput", ADSRParamItemGateInput, 0, 0, nullptr, 0.0f, 3.4e38f, 0.0f, 0},
   {"legato",    ADSRParamItemLegato,    0, 0, nullptr, 0.0f, 2.0f, 0.0f, 0},
   {"linear",    ADSRParamItemLinear,    0, 0, nullptr, 0.0f, 1.0f, 0.0f, 0},
};

const ModuleParamConstantsGroup adsrConstantsGroup = {
   const_cast<char*>("adsr"),
   sizeof(adsrConstants) / sizeof(adsrConstants[0]),
   adsrConstants,
};

// Combine — 4-quadrant multiplier (the VCA). Type defaults to Multiply, Level 1.0.
static const ModuleParamConstants combineConstants[] = {
   {"input1", CombineParamItemInput1, 0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"input2", CombineParamItemInput2, 0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"input3", CombineParamItemInput3, 0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"type", CombineParamItemType, 0, 0, nullptr, (float)ParamItemCombineType_Add,
    (float)ParamItemCombineType_Multiply, (float)ParamItemCombineType_Multiply, 0},
   {"level", CombineParamItemLevel, 0, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
};
const ModuleParamConstantsGroup combineConstantsGroup = {
   const_cast<char*>("combine"), sizeof(combineConstants) / sizeof(combineConstants[0]),
   combineConstants};

// Final — output stage (DC block / level / M-S spread / clip).
static const ModuleParamConstants finalConstants[] = {
   {"input", FinalOutputParamItemInput, 0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"spread", FinalOutputParamItemSpread, 0, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"level", FinalOutputParamItemLevel, 0, 0, nullptr, 0.0f, 1.0f, 0.7f, 0},
   {"clipLevel", FinalOutputParamItemClipLevel, 0, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
   {"filterDC", FinalOutputParamItemFilterDC, 0, 0, nullptr, 0.0f, 1.0f, 1.0f, 0},
};
const ModuleParamConstantsGroup finalConstantsGroup = {
   const_cast<char*>("final"), sizeof(finalConstants) / sizeof(finalConstants[0]), finalConstants};

// Slew — slew-rate limiter (smooths the pitch signal). Rise/Fall in seconds per unit.
static const ModuleParamConstants slewConstants[] = {
   {"input", SlewParamItemInput, 0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
   {"rise", SlewParamItemRise, 0, 0, nullptr, 1e-8f, 10.0f, 0.01f, 0},
   {"fall", SlewParamItemFall, 0, 0, nullptr, 1e-8f, 10.0f, 0.01f, 0},
   {"curve", SlewParamItemCurve, 0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup slewModuleConstantsGroup = {
   const_cast<char*>("slew"), sizeof(slewConstants) / sizeof(slewConstants[0]), slewConstants};

// PitchConverter — MIDI note → pitch (1/octave). One placeholder param.
static const ModuleParamConstants pitchConverterConstants[] = {
   {"dummy", PitchConverterParamItemDummy, 0, 0, nullptr, 0.0f, 0.0f, 0.0f, 0},
};
const ModuleParamConstantsGroup pitchConverterModuleConstantsGroup = {
   const_cast<char*>("pitchConverter"),
   sizeof(pitchConverterConstants) / sizeof(pitchConverterConstants[0]), pitchConverterConstants};
