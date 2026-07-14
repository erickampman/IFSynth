// IFSynthParams.h — the plugin's parameter table, shared by the DSP and UI sides so
// they agree on index, name, range and default. v1 exposes master gain plus the four ADSR
// controls of the (single) envelope; the ADSR ones route straight to the VoiceManager's
// ADSR module. Time controls use a logarithmic law (natural for envelope times).

#ifndef IFSYNTH_PARAMS_H
#define IFSYNTH_PARAMS_H

namespace ifsynth {

enum ParamIndex {
   kParamGain = 0,
   kParamAttack,
   kParamDecay,
   kParamSustain,
   kParamRelease,
   kParamCount
};

struct ParamSpec {
   const char* name;
   const char* symbol;
   const char* unit;
   float       min;
   float       max;
   float       def;
   bool        logarithmic; // time knobs span 0.001..10 s → log feels right
};

// Ranges/defaults mirror adsrConstants[] in pm_param_data.cpp.
static constexpr ParamSpec kParams[kParamCount] = {
   {"Gain",    "gain",    "",  0.0f,   1.0f,  0.5f,  false},
   {"Attack",  "attack",  "s", 0.001f, 10.0f, 0.01f, true },
   {"Decay",   "decay",   "s", 0.001f, 10.0f, 1.0f,  true },
   {"Sustain", "sustain", "",  0.0f,   1.0f,  0.5f,  false},
   {"Release", "release", "s", 0.001f, 10.0f, 1.0f,  true },
};

} // namespace ifsynth

#endif // IFSYNTH_PARAMS_H
