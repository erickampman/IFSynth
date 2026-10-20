// DistrhoPluginInfo.h — compile-time description of the IFSynth plugin for DPF.
//
// IMPORTANT: the DISTRHO_PLUGIN_* *identity* macros below are INERT. IFSynth does NOT use
// dpf_add_plugin: it implements the CLAP entry point itself in src/plugin.cpp. That is deliberate
// and load-bearing — DPF's generated CLAP wrapper ignores CLAP_EVENT_MIDI2 outright
// (dpf/distrho/src/DistrhoPluginCLAP.cpp: "case CLAP_EVENT_MIDI2: break;") and advertises only
// CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI_MPE, so a DPF-generated plugin can never receive
// MIDI 2.0 7.9-pitch NoteOns. See src/plugin.cpp's note-ports extension.
//
// The DISTRHO_UI_* macros DO bind: DPF is used here purely as the GUI layer (NanoVG widgets driven
// through UIExporter), exactly as PlainOldSynth does it.

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
// Gates UI::getPluginInstancePointer() (DistrhoUI.hpp), which the GUI shell uses to recover
// the clap_plugin_t* UIExporter was constructed with. Inert for the raw CLAP plugin itself.
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1

#define DISTRHO_UI_USE_NANOVG          1   // knobs/labels drawn with NanoVG vector primitives
#define DISTRHO_UI_WEB_VIEW           1   // Help button: native overlay (WKWebView/WebKitGTK/WebView2).
                                          // DPF compiles its WebView impl from DistrhoUI_macOS.mm /
                                          // DistrhoUI.cpp / DistrhoUI_win32.cpp, not from dgl.
#define DISTRHO_UI_USER_RESIZABLE      1
#define DISTRHO_UI_DEFAULT_WIDTH       980
#define DISTRHO_UI_DEFAULT_HEIGHT      780   // 4 rows (Env+LFO is a tab group); row 0 is ~20 px
                                             // taller, row 2 ~40 px (larger Amp knobs)

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
