// IFSynthParams.h — the plugin's parameter table, shared by the DSP and UI sides so they agree
// on index, name, range and default. Master gain + the ADSR envelope + the IFS oscillator's
// tune, the five fractal-shape controls (edited by the IFSControl pad), smoothing and
// iterations. Non-gain params route to the matching VoiceManager module item (see the DSP's
// vmAddress()). Shape defaults mirror ifsOscConstants[] / IFSShapeParams{}.

#ifndef IFSYNTH_PARAMS_H
#define IFSYNTH_PARAMS_H

namespace ifsynth {

enum ParamIndex {
   kParamGain = 0,
   // ADSR envelope
   kParamAttack,
   kParamDecay,
   kParamSustain,
   kParamRelease,
   // IFS oscillator — tune
   kParamOscPitch,
   kParamOscFine,
   // IFS oscillator — fractal shape (edited by the IFSControl pad)
   kParamBx,
   kParamBy,
   kParamStartEndY,
   kParamIFS1Squash,
   kParamIFS2Squash,
   // IFS oscillator — timbre
   kParamSmoothing,
   kParamIterations,
   kParamCount
};

struct ParamSpec {
   const char* name;
   const char* symbol;
   const char* unit;
   float       min;
   float       max;
   float       def;
   bool        logarithmic; // envelope times span 0.001..10 s → log law
   bool        integer;     // stepped (iterations)
};

static constexpr ParamSpec kParams[kParamCount] = {
   {"Gain",       "gain",      "",   0.0f,   1.0f,   0.5f,  false, false},
   {"Attack",     "attack",    "s",  0.001f, 10.0f,  0.01f, true,  false},
   {"Decay",      "decay",     "s",  0.001f, 10.0f,  1.0f,  true,  false},
   {"Sustain",    "sustain",   "",   0.0f,   1.0f,   0.5f,  false, false},
   {"Release",    "release",   "s",  0.001f, 10.0f,  1.0f,  true,  false},
   {"Osc Pitch",  "oscPitch",  "st", -24.0f, 24.0f,  0.0f,  false, false},
   {"Osc Fine",   "oscFine",   "ct", -100.0f,100.0f, 0.0f,  false, false},
   {"Split",      "bx",        "",   -1.0f,  1.0f,   0.0f,  false, false},
   {"Junction",   "by",        "",   -1.0f,  1.0f,   0.5f,  false, false},
   {"Ends",       "startEndY", "",   -1.0f,  1.0f,  -1.0f,  false, false},
   {"Squash 1",   "ifs1Squash","",   -0.99f, 0.99f,  0.5f,  false, false},
   {"Squash 2",   "ifs2Squash","",   -0.99f, 0.99f, -0.5f,  false, false},
   {"Smoothing",  "smoothing", "",   0.0f,   1.0f,   0.5f,  false, false},
   {"Iterations", "iterations","",   0.0f,   1.0f,   1.0f,  false, true },
};

} // namespace ifsynth

#endif // IFSYNTH_PARAMS_H
