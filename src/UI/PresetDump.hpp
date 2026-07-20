// PresetDump.hpp — DEV-ONLY. Writes the current patch as module-nested JSON (PM-style) to a
// timestamped file in $HOME. Compiled in only when IFSYNTH_PRESET_DUMP=1 (see CMake option); the
// UI triggers it on a keystroke. The kPresetKeys table maps each plugin param to a
// (module-instance, item) pair — the same table is the natural basis for a future loader.

#ifndef IFSYNTH_PRESETDUMP_HPP
#define IFSYNTH_PRESETDUMP_HPP

#if IFSYNTH_PRESET_DUMP

#include "IFSynthParams.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

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

// Emit modules in this order (signal-flow-ish); every module in kPresetKeys must appear here.
static constexpr const char* kPresetModuleOrder[] = {
   "Global", "SinOsc1", "IFSOsc1", "LPF1", "IFSEnv1", "IFSEnv2", "IFSLFO1", "IFSLFO2",
   "CVMixer1", "CVMixer2",
};

// Build the module-nested JSON for `values` (kParamCount entries). Values are stringified like PM
// (ints as "0"/"1", floats to 6 dp).
inline std::string buildPresetJson(const float* values, const char* name = "Untitled")
{
   char iso[32] = "";
   const std::time_t t = std::time(nullptr);
   std::strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));

   std::string j;
   j += "{\n";
   j += "  \"app\": { \"name\": \"IFSynth\", \"version\": \"0.4.0\" },\n";
   j += "  \"plugin\": { \"format\": \"clap\", \"id\": \"com.example.ifsynth\" },\n";
   j += "  \"meta\": { \"author\": \"\", \"createdAt\": \"" + std::string(iso)
        + "\", \"name\": \"" + name + "\" },\n";
   j += "  \"schema\": \"ifsynth.preset\",\n";
   j += "  \"schemaVersion\": 1,\n";
   j += "  \"state\": {\n";
   j += "    \"parameterState\": {\n";

   const int nMods = static_cast<int>(sizeof(kPresetModuleOrder) / sizeof(kPresetModuleOrder[0]));
   for (int m = 0; m < nMods; ++m)
   {
      const char* mod = kPresetModuleOrder[m];
      j += "      \"" + std::string(mod) + "\": {\n";
      bool first = true;
      for (uint32_t i = 0; i < kParamCount; ++i)
      {
         if (std::strcmp(kPresetKeys[i].module, mod) != 0)
            continue;
         char val[32];
         if (kParams[i].integer)
            std::snprintf(val, sizeof val, "%d", static_cast<int>(std::lround(values[i])));
         else
            std::snprintf(val, sizeof val, "%.6f", values[i]);
         if (!first) j += ",\n";
         first = false;
         j += "        \"" + std::string(kPresetKeys[i].item) + "\": \"" + val + "\"";
      }
      j += "\n      }";
      j += (m + 1 < nMods) ? ",\n" : "\n";
   }

   j += "    }\n";
   j += "  }\n";
   j += "}\n";
   return j;
}

// Write the patch to $HOME/ifsynth-preset-<epoch>.json. Returns the path, or "" on failure.
inline std::string dumpPresetToFile(const float* values)
{
   const std::string json = buildPresetJson(values);
   const char* home = std::getenv("HOME");
   std::string path = std::string(home ? home : ".") + "/ifsynth-preset-"
                    + std::to_string(static_cast<long long>(std::time(nullptr))) + ".json";
   FILE* f = std::fopen(path.c_str(), "w");
   if (f == nullptr)
      return "";
   std::fwrite(json.data(), 1, json.size(), f);
   std::fclose(f);
   return path;
}

} // namespace ifsynth

#endif // IFSYNTH_PRESET_DUMP
#endif // IFSYNTH_PRESETDUMP_HPP
