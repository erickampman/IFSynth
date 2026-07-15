// IFSynthParams.h — the plugin's parameter table, shared by the DSP and UI. Master gain, the
// IFS oscillator (tune + fractal shape + smoothing/iterations), and the IFS envelope (duration
// + 3-transform shape + smoothing/iterations/hold). Non-gain params route to the matching
// VoiceManager module item (see the DSP's vmAddress). Shape defaults mirror the module
// constants in pm_param_data.cpp.

#ifndef IFSYNTH_PARAMS_H
#define IFSYNTH_PARAMS_H

namespace ifsynth {

enum ParamIndex {
   kParamGain = 0,
   // IFS oscillator
   kParamOscPitch,
   kParamOscFine,
   kParamOscBx,
   kParamOscBy,
   kParamOscStartEndY,
   kParamOscSquash1,
   kParamOscSquash2,
   kParamOscSmoothing,
   kParamOscIterations,
   // IFS envelope
   kParamEnvDuration,
   kParamEnvStartEndY,
   kParamEnvBlueX,
   kParamEnvBlueY,
   kParamEnvGreenX,
   kParamEnvGreenY,
   kParamEnvSquash1,
   kParamEnvSquash2,
   kParamEnvSquash3,
   kParamEnvSmoothing,
   kParamEnvIterations,
   kParamEnvHoldMode,
   kParamCount
};

struct ParamSpec {
   const char* name;
   const char* symbol;
   const char* unit;
   float       min;
   float       max;
   float       def;
   bool        logarithmic;
   bool        integer;
};

static constexpr ParamSpec kParams[kParamCount] = {
   {"Gain",        "gain",       "",   0.0f,   1.0f,   0.5f,  false, false},
   // IFS oscillator
   {"Osc Pitch",   "oscPitch",   "st", -24.0f, 24.0f,  0.0f,  false, false},
   {"Osc Fine",    "oscFine",    "ct", -100.0f,100.0f, 0.0f,  false, false},
   {"Split",       "oscBx",      "",   -1.0f,  1.0f,   0.0f,  false, false},
   {"Junction",    "oscBy",      "",   -1.0f,  1.0f,   0.5f,  false, false},
   {"Ends",        "oscEnds",    "",   -1.0f,  1.0f,  -1.0f,  false, false},
   {"Squash 1",    "oscSq1",     "",   -0.99f, 0.99f,  0.5f,  false, false},
   {"Squash 2",    "oscSq2",     "",   -0.99f, 0.99f, -0.5f,  false, false},
   {"Smoothing",   "oscSmooth",  "",   0.0f,   1.0f,   0.5f,  false, false},
   {"Iterations",  "oscIters",   "",   0.0f,   1.0f,   1.0f,  false, true },
   // IFS envelope
   {"Duration",    "envDur",     "s",  0.01f,  10.0f,  1.0f,  true,  false},
   {"Ends",        "envEnds",    "",   -1.0f,  1.0f,   0.0f,  false, false},
   {"Blue X",      "envBlueX",   "",   0.0f,   1.0f,   0.15f, false, false},
   {"Blue Y",      "envBlueY",   "",   -1.0f,  1.0f,   1.0f,  false, false},
   {"Green X",     "envGreenX",  "",   0.0f,   1.0f,   0.45f, false, false},
   {"Green Y",     "envGreenY",  "",   -1.0f,  1.0f,   0.5f,  false, false},
   {"Squash 1",    "envSq1",     "",   -0.99f, 0.99f,  0.2f,  false, false},
   {"Squash 2",    "envSq2",     "",   -0.99f, 0.99f,  0.2f,  false, false},
   {"Squash 3",    "envSq3",     "",   -0.99f, 0.99f,  0.2f,  false, false},
   {"Smoothing",   "envSmooth",  "",   0.0f,   1.0f,   0.5f,  false, false},
   {"Iterations",  "envIters",   "",   0.0f,   7.0f,   2.0f,  false, true },
   {"Hold",        "envHold",    "",   0.0f,   3.0f,   2.0f,  false, true },
};

} // namespace ifsynth

#endif // IFSYNTH_PARAMS_H
