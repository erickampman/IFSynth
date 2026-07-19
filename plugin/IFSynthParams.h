// IFSynthParams.h — the plugin's parameter table, shared by the DSP and UI. Master gain, the
// IFS oscillator (tune + fractal shape + smoothing/iterations), and the IFS envelope (duration
// + 3-transform shape + smoothing/iterations/hold). Non-gain params route to the matching
// VoiceManager module item (see the DSP's vmAddress). Shape defaults mirror the module
// constants in pm_param_data.cpp.

#ifndef IFSYNTH_PARAMS_H
#define IFSYNTH_PARAMS_H

namespace ifsynth {

enum ParamIndex {
   kParamVoiceMode = 0, // 0 = Mono (1 voice + legato note-stack + glide), 1 = 10-voice poly
   kParamSlewRate,   // pitch glide — drives the Slew module's Rise and Fall together
   // IFS oscillator
   kParamOscPitch,
   kParamOscFine,
   kParamOscDetune,   // stereo detune (cents), splits L/R
   kParamOscBx,
   kParamOscBy,
   kParamOscStartEndY,
   kParamOscSquash1,
   kParamOscSquash2,
   kParamOscSmoothing,
   kParamOscIterations,
   // IFS envelope (always starts/ends at 0 — no start/end param)
   kParamEnvDuration,
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
   // IFS LFO (not yet wired to the graph)
   kParamLfoRate,
   kParamLfoBx,
   kParamLfoBy,
   kParamLfoStartEndY,
   kParamLfoSquash1,
   kParamLfoSquash2,
   kParamLfoSmoothing,
   kParamLfoIterations,
   // IFS envelope 2 (mod source, not wired to the graph)
   kParamEnv2Duration,
   kParamEnv2BlueX,
   kParamEnv2BlueY,
   kParamEnv2GreenX,
   kParamEnv2GreenY,
   kParamEnv2Squash1,
   kParamEnv2Squash2,
   kParamEnv2Squash3,
   kParamEnv2Smoothing,
   kParamEnv2Iterations,
   kParamEnv2HoldMode,
   // IFS LFO 2 (mod source, not wired to the graph)
   kParamLfo2Rate,
   kParamLfo2Bx,
   kParamLfo2By,
   kParamLfo2StartEndY,
   kParamLfo2Squash1,
   kParamLfo2Squash2,
   kParamLfo2Smoothing,
   kParamLfo2Iterations,
   // CV mixer 1 (combines LFO1 + Env1 + CC)
   kParamMix1EnableLFO,
   kParamMix1EnableEnv,
   kParamMix1EnableCC,
   kParamMix1CCNumber,
   kParamMix1Mode,
   // CV mixer 2 (combines LFO2 + Env2 + CC)
   kParamMix2EnableLFO,
   kParamMix2EnableEnv,
   kParamMix2EnableCC,
   kParamMix2CCNumber,
   kParamMix2Mode,
   // Modulation routing: per IFSOsc mod input, a source select (0=None,1..6) + bipolar depth.
   kParamOscPitchModSrc, kParamOscPitchModDepth,
   kParamOscBxModSrc,    kParamOscBxModDepth,
   kParamOscByModSrc,    kParamOscByModDepth,
   kParamOscSq1ModSrc,   kParamOscSq1ModDepth,
   kParamOscSq2ModSrc,   kParamOscSq2ModDepth,
   // The amplitude VCA's envelope source (Env 1 / Env 2), no depth.
   kParamVCASource,
   // Low-pass filter (24 dB/oct Butterworth, after the oscillator, before the VCA)
   kParamLpfCutoff,
   kParamLpfResonance,
   kParamLpfKeyTrack,        // cutoff follows note pitch (0 = none, 1 = 1:1)
   // LPF cutoff modulation: a source select (0=None,1..6) + bipolar depth (own bordered panel).
   kParamLpfCutoffModSrc,
   kParamLpfCutoffModDepth,
   kParamSpread,      // output stereo width (0 = mono, 1 = full); pairs with Osc Detune
   // Sine oscillator (hard-sync master)
   kParamSinPitch,
   kParamSinFine,
   kParamSinDetune,   // stereo detune (cents), drives per-channel IFS sync
   // Amp mix levels (replace the old global gain) + IFS hard-sync mode
   kParamSinLevel,    // sine osc output level
   kParamIfsLevel,    // IFS osc output level
   kParamIfsSyncMode, // 0 Off, 1 Up, 2 Down, 3 Both
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
   {"Voices",      "voices",     "",   0.0f,   1.0f,   1.0f,  false, true },
   {"Glide",       "glide",      "s",  0.0001f, 2.0f,  0.01f, true,  false},
   // IFS oscillator
   {"Osc Pitch",   "oscPitch",   "st", -24.0f, 24.0f,  0.0f,  false, false},
   {"Osc Fine",    "oscFine",    "ct", -100.0f,100.0f, 0.0f,  false, false},
   {"Detune",      "oscDetune",  "ct", 0.0f,   25.0f,  0.0f,  false, false},
   {"Split",       "oscBx",      "",   -1.0f,  1.0f,   0.0f,  false, false},
   {"Junction",    "oscBy",      "",   -1.0f,  1.0f,   0.5f,  false, false},
   {"Ends",        "oscEnds",    "",   -1.0f,  1.0f,  -1.0f,  false, false},
   {"Squash 1",    "oscSq1",     "",   -0.99f, 0.99f,  0.5f,  false, false},
   {"Squash 2",    "oscSq2",     "",   -0.99f, 0.99f, -0.5f,  false, false},
   {"Smoothing",   "oscSmooth",  "",   0.0f,   1.0f,   0.5f,  false, false},
   {"Iterations",  "oscIters",   "",   0.0f,   1.0f,   1.0f,  false, true },
   // IFS envelope
   {"Duration",    "envDur",     "s",  0.01f,  10.0f,  1.0f,  true,  false},
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
   // IFS LFO
   {"Rate",        "lfoRate",    "oct",-7.0f,  7.0f,   0.0f,  false, false},
   {"Split",       "lfoBx",      "",   -1.0f,  1.0f,   0.0f,  false, false},
   {"Junction",    "lfoBy",      "",   -1.0f,  1.0f,   0.5f,  false, false},
   {"Ends",        "lfoEnds",    "",   -1.0f,  1.0f,  -1.0f,  false, false},
   {"Squash 1",    "lfoSq1",     "",   -0.99f, 0.99f,  0.5f,  false, false},
   {"Squash 2",    "lfoSq2",     "",   -0.99f, 0.99f, -0.5f,  false, false},
   {"Smoothing",   "lfoSmooth",  "",   0.0f,   1.0f,   0.5f,  false, false},
   {"Iterations",  "lfoIters",   "",   0.0f,   7.0f,   3.0f,  false, true },
   // IFS envelope 2
   {"Duration",    "env2Dur",    "s",  0.01f,  10.0f,  1.0f,  true,  false},
   {"Blue X",      "env2BlueX",  "",   0.0f,   1.0f,   0.15f, false, false},
   {"Blue Y",      "env2BlueY",  "",   -1.0f,  1.0f,   1.0f,  false, false},
   {"Green X",     "env2GreenX", "",   0.0f,   1.0f,   0.45f, false, false},
   {"Green Y",     "env2GreenY", "",   -1.0f,  1.0f,   0.5f,  false, false},
   {"Squash 1",    "env2Sq1",    "",   -0.99f, 0.99f,  0.2f,  false, false},
   {"Squash 2",    "env2Sq2",    "",   -0.99f, 0.99f,  0.2f,  false, false},
   {"Squash 3",    "env2Sq3",    "",   -0.99f, 0.99f,  0.2f,  false, false},
   {"Smoothing",   "env2Smooth", "",   0.0f,   1.0f,   0.5f,  false, false},
   {"Iterations",  "env2Iters",  "",   0.0f,   7.0f,   2.0f,  false, true },
   {"Hold",        "env2Hold",   "",   0.0f,   3.0f,   2.0f,  false, true },
   // IFS LFO 2
   {"Rate",        "lfo2Rate",   "oct",-7.0f,  7.0f,   0.0f,  false, false},
   {"Split",       "lfo2Bx",     "",   -1.0f,  1.0f,   0.0f,  false, false},
   {"Junction",    "lfo2By",     "",   -1.0f,  1.0f,   0.5f,  false, false},
   {"Ends",        "lfo2Ends",   "",   -1.0f,  1.0f,  -1.0f,  false, false},
   {"Squash 1",    "lfo2Sq1",    "",   -0.99f, 0.99f,  0.5f,  false, false},
   {"Squash 2",    "lfo2Sq2",    "",   -0.99f, 0.99f, -0.5f,  false, false},
   {"Smoothing",   "lfo2Smooth", "",   0.0f,   1.0f,   0.5f,  false, false},
   {"Iterations",  "lfo2Iters",  "",   0.0f,   7.0f,   3.0f,  false, true },
   // CV mixer 1
   {"LFO 1",       "mix1LFO",    "",   0.0f,   1.0f,   1.0f,  false, true },
   {"Env 1",       "mix1Env",    "",   0.0f,   1.0f,   1.0f,  false, true },
   {"CC",          "mix1CCen",   "",   0.0f,   1.0f,   0.0f,  false, true },
   {"CC #",        "mix1CCnum",  "",   1.0f, 127.0f,   1.0f,  false, true },
   {"Mode",        "mix1Mode",   "",   0.0f,   1.0f,   0.0f,  false, true },
   // CV mixer 2
   {"LFO 2",       "mix2LFO",    "",   0.0f,   1.0f,   1.0f,  false, true },
   {"Env 2",       "mix2Env",    "",   0.0f,   1.0f,   1.0f,  false, true },
   {"CC",          "mix2CCen",   "",   0.0f,   1.0f,   0.0f,  false, true },
   {"CC #",        "mix2CCnum",  "",   1.0f, 127.0f,   1.0f,  false, true },
   {"Mode",        "mix2Mode",   "",   0.0f,   1.0f,   0.0f,  false, true },
   // Modulation routing (source = int 0..6; depth = bipolar)
   {"Pitch",       "oscPMsrc",   "",   0.0f,   6.0f,   0.0f,  false, true },
   {"Depth",       "oscPMdpt",   "",  -1.0f,   1.0f,   0.0f,  false, false},
   {"Split",       "oscBxMsrc",  "",   0.0f,   6.0f,   0.0f,  false, true },
   {"Depth",       "oscBxMdpt",  "",  -1.0f,   1.0f,   0.0f,  false, false},
   {"Junction",    "oscByMsrc",  "",   0.0f,   6.0f,   0.0f,  false, true },
   {"Depth",       "oscByMdpt",  "",  -1.0f,   1.0f,   0.0f,  false, false},
   {"Squash 1",    "oscSq1Msrc", "",   0.0f,   6.0f,   0.0f,  false, true },
   {"Depth",       "oscSq1Mdpt", "",  -1.0f,   1.0f,   0.0f,  false, false},
   {"Squash 2",    "oscSq2Msrc", "",   0.0f,   6.0f,   0.0f,  false, true },
   {"Depth",       "oscSq2Mdpt", "",  -1.0f,   1.0f,   0.0f,  false, false},
   {"VCA env",     "vcaSrc",     "",   0.0f,   1.0f,   0.0f,  false, true },
   // Low-pass filter. Cutoff is octaves above A440 (pitchToFrequency); Resonance is Q (1..20);
   // Key Track scales how much the cutoff follows the note (1 = 1:1). Cutoff mod = source + depth.
   {"Cutoff",      "lpfCutoff",  "",   -6.0f,  5.4f,   5.0f,  false, false},
   {"Resonance",   "lpfReso",    "",   1.0f,  20.0f,   1.0f,  false, false},
   {"Key Track",   "lpfKeyTrk",  "",   0.0f,   1.0f,   0.0f,  false, false},
   {"Cutoff",      "lpfCMsrc",   "",   0.0f,   6.0f,   0.0f,  false, true },
   {"Depth",       "lpfCMdpt",   "",  -1.0f,   1.0f,   0.0f,  false, false},
   {"Spread",      "spread",     "",   0.0f,   1.0f,   1.0f,  false, false},
   // Sine oscillator (master). Pitch is an interval selector (index → semitones in the DSP),
   // default 3 = unison; see the applyParam mapping.
   {"Pitch",       "sinPitch",   "",   0.0f,   6.0f,   3.0f,  false, true },
   {"Fine",        "sinFine",    "ct", -100.0f,100.0f, 0.0f,  false, false},
   {"Detune",      "sinDetune",  "ct", 0.0f,   25.0f,  0.0f,  false, false},
   // Amp mix levels + IFS hard-sync mode
   {"Sine",        "sinLevel",   "",   0.0f,   1.0f,   0.0f,  false, false},
   {"IFS",         "ifsLevel",   "",   0.0f,   1.0f,   0.5f,  false, false},
   {"Sync",        "ifsSync",    "",   0.0f,   3.0f,   0.0f,  false, true },
};

} // namespace ifsynth

#endif // IFSYNTH_PARAMS_H
