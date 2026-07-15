// DistrhoPluginInfo.h — compile-time description of the IFSynth plugin for DPF.
// DPF reads these macros to generate the CLAP (and, if enabled, VST3/LV2) wrapper around
// our Plugin/UI subclasses. IS_SYNTH gives it a note-input port + the CLAP "instrument"
// feature, and pulls in MIDI input automatically.

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND       "example"
#define DISTRHO_PLUGIN_NAME        "IFSynth"
#define DISTRHO_PLUGIN_URI         "https://example.com/ifsynth"
#define DISTRHO_PLUGIN_CLAP_ID     "com.example.ifsynth"
#define DISTRHO_PLUGIN_CLAP_FEATURES "instrument", "synthesizer", "stereo"
#define DISTRHO_PLUGIN_BRAND_ID    Exmp
#define DISTRHO_PLUGIN_UNIQUE_ID   IFSy

#define DISTRHO_PLUGIN_HAS_UI          1
#define DISTRHO_PLUGIN_IS_SYNTH        1   // note input + CLAP "instrument"; enables MIDI in
#define DISTRHO_PLUGIN_IS_RT_SAFE      1
#define DISTRHO_PLUGIN_NUM_INPUTS      0
#define DISTRHO_PLUGIN_NUM_OUTPUTS     2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT 1
#define DISTRHO_PLUGIN_WANT_PROGRAMS   0
#define DISTRHO_PLUGIN_WANT_STATE      0

#define DISTRHO_UI_USE_NANOVG          1   // knobs/labels drawn with NanoVG vector primitives
#define DISTRHO_UI_USER_RESIZABLE      1
#define DISTRHO_UI_DEFAULT_WIDTH       980
#define DISTRHO_UI_DEFAULT_HEIGHT      700

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
