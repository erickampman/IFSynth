// PresetKeys.hpp — the plugin-param ↔ preset-JSON-field mapping, shared by the dev-only JSON
// dumper (PresetDump.hpp) and the always-on preset loader (GeneratedPresets.h resolution in
// IFSynthUI.cpp). Kept in its own always-compiled header (not gated by IFSYNTH_PRESET_DUMP)
// since preset loading must work in every build, not just dev builds.

#ifndef IFSYNTH_PRESETKEYS_HPP
#define IFSYNTH_PRESETKEYS_HPP

#include "IFSynthParams.h"

namespace ifsynth {

struct PresetKey { const char* module; const char* item; };

// One entry per param, index-aligned to ParamIndex. Item names follow the PM module constants.
static constexpr PresetKey kPresetKeys[kParamCount] = {
   /*VoiceMode*/       {"Global",  "voiceMode"},
   /*SlewRate*/        {"Global",  "glide"},
   /*OscPitch*/        {"IFSOsc1", "pitch"},
   /*OscFine*/         {"IFSOsc1", "fine"},
   /*OscDetune*/       {"IFSOsc1", "detune"},
   /*OscBx*/           {"IFSOsc1", "bx"},
   /*OscBy*/           {"IFSOsc1", "by"},
   /*OscStartEndY*/    {"IFSOsc1", "startEndY"},
   /*OscSquash1*/      {"IFSOsc1", "ifs1Squash"},
   /*OscSquash2*/      {"IFSOsc1", "ifs2Squash"},
   /*OscSmoothing*/    {"IFSOsc1", "smoothing"},
   /*OscIterations*/   {"IFSOsc1", "iterations"},
   /*EnvDuration*/     {"IFSEnv1", "duration"},
   /*EnvBlueX*/        {"IFSEnv1", "blueX"},
   /*EnvBlueY*/        {"IFSEnv1", "blueY"},
   /*EnvGreenX*/       {"IFSEnv1", "greenX"},
   /*EnvGreenY*/       {"IFSEnv1", "greenY"},
   /*EnvSquash1*/      {"IFSEnv1", "ifs1Squash"},
   /*EnvSquash2*/      {"IFSEnv1", "ifs2Squash"},
   /*EnvSquash3*/      {"IFSEnv1", "ifs3Squash"},
   /*EnvSmoothing*/    {"IFSEnv1", "smoothing"},
   /*EnvIterations*/   {"IFSEnv1", "iterations"},
   /*EnvHoldMode*/     {"IFSEnv1", "holdMode"},
   /*LfoRate*/         {"IFSLFO1", "frequency"},
   /*LfoBx*/           {"IFSLFO1", "bx"},
   /*LfoBy*/           {"IFSLFO1", "by"},
   /*LfoStartEndY*/    {"IFSLFO1", "startEndY"},
   /*LfoSquash1*/      {"IFSLFO1", "ifs1Squash"},
   /*LfoSquash2*/      {"IFSLFO1", "ifs2Squash"},
   /*LfoSmoothing*/    {"IFSLFO1", "smoothing"},
   /*LfoIterations*/   {"IFSLFO1", "iterations"},
   /*Env2Duration*/    {"IFSEnv2", "duration"},
   /*Env2BlueX*/       {"IFSEnv2", "blueX"},
   /*Env2BlueY*/       {"IFSEnv2", "blueY"},
   /*Env2GreenX*/      {"IFSEnv2", "greenX"},
   /*Env2GreenY*/      {"IFSEnv2", "greenY"},
   /*Env2Squash1*/     {"IFSEnv2", "ifs1Squash"},
   /*Env2Squash2*/     {"IFSEnv2", "ifs2Squash"},
   /*Env2Squash3*/     {"IFSEnv2", "ifs3Squash"},
   /*Env2Smoothing*/   {"IFSEnv2", "smoothing"},
   /*Env2Iterations*/  {"IFSEnv2", "iterations"},
   /*Env2HoldMode*/    {"IFSEnv2", "holdMode"},
   /*Lfo2Rate*/        {"IFSLFO2", "frequency"},
   /*Lfo2Bx*/          {"IFSLFO2", "bx"},
   /*Lfo2By*/          {"IFSLFO2", "by"},
   /*Lfo2StartEndY*/   {"IFSLFO2", "startEndY"},
   /*Lfo2Squash1*/     {"IFSLFO2", "ifs1Squash"},
   /*Lfo2Squash2*/     {"IFSLFO2", "ifs2Squash"},
   /*Lfo2Smoothing*/   {"IFSLFO2", "smoothing"},
   /*Lfo2Iterations*/  {"IFSLFO2", "iterations"},
   /*Mix1EnableLFO*/   {"CVMixer1", "enableLFO"},
   /*Mix1EnableEnv*/   {"CVMixer1", "enableEnv"},
   /*Mix1EnableCC*/    {"CVMixer1", "enableCC"},
   /*Mix1CCNumber*/    {"CVMixer1", "ccNumber"},
   /*Mix1Mode*/        {"CVMixer1", "mode"},
   /*Mix2EnableLFO*/   {"CVMixer2", "enableLFO"},
   /*Mix2EnableEnv*/   {"CVMixer2", "enableEnv"},
   /*Mix2EnableCC*/    {"CVMixer2", "enableCC"},
   /*Mix2CCNumber*/    {"CVMixer2", "ccNumber"},
   /*Mix2Mode*/        {"CVMixer2", "mode"},
   /*OscPitchModSrc*/  {"IFSOsc1", "pitchModSrc"},
   /*OscPitchModDepth*/{"IFSOsc1", "pitchModLevel"},
   /*OscBxModSrc*/     {"IFSOsc1", "bxModSrc"},
   /*OscBxModDepth*/   {"IFSOsc1", "bxModLevel"},
   /*OscByModSrc*/     {"IFSOsc1", "byModSrc"},
   /*OscByModDepth*/   {"IFSOsc1", "byModLevel"},
   /*OscSq1ModSrc*/    {"IFSOsc1", "sq1ModSrc"},
   /*OscSq1ModDepth*/  {"IFSOsc1", "sq1ModLevel"},
   /*OscSq2ModSrc*/    {"IFSOsc1", "sq2ModSrc"},
   /*OscSq2ModDepth*/  {"IFSOsc1", "sq2ModLevel"},
   /*VCASource*/       {"Global",  "vcaEnv"},
   /*LpfCutoff*/       {"LPF1",    "frequency"},
   /*LpfResonance*/    {"LPF1",    "q"},
   /*LpfKeyTrack*/     {"LPF1",    "keyTrack"},
   /*LpfCutoffModSrc*/ {"LPF1",    "cutoffModSrc"},
   /*LpfCutoffModDepth*/{"LPF1",   "cutoffModLevel"},
   /*Spread*/          {"Global",  "spread"},
   /*SinPitch*/        {"SinOsc1", "pitch"},
   /*SinFine*/         {"SinOsc1", "fine"},
   /*SinDetune*/       {"SinOsc1", "detune"},
   /*SinWaveform*/     {"SinOsc1", "waveform"},
   /*SinLevel*/        {"Global",  "sinLevel"},
   /*IfsLevel*/        {"Global",  "ifsLevel"},
   /*IfsSyncMode*/     {"IFSOsc1", "syncMode"},
   /*Osc2PitchModSrc*/ {"SinOsc1", "pitchModSrc"},
   /*Osc2PitchModDepth*/{"SinOsc1","freqModLevel"},
};

} // namespace ifsynth

#endif // IFSYNTH_PRESETKEYS_HPP
