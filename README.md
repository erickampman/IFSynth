# IFSynth

A cross-platform **CLAP fractal synth** — the IFS (Iterated Function System) instrument built
on top of the [SimpleSynth](https://github.com/erickampman/SimpleSynth) base project. IFSynth
carries SimpleSynth's PolygraphModular voice engine + DPF/NanoVG UI kit unchanged and adds the
IFS module family (IFSOsc / IFSEnv / IFSLFO) and their custom fractal-editor control. SimpleSynth
stays a clean, IFS-free base; this repo is where the fractal work lives.

## Status

**Polyphonic synth with a GUI, running the real PolygraphModular voice engine.** The PM
framework is vendored off AUv3 and drives a `VoiceManager` (polyphony, note-stealing, mono
mode) rendering the v1 voice **PitchConverter → Slew → SinOsc → Combine ×ADSR → Final** per
voice. The plugin wrapper is now built with the [DISTRHO Plugin Framework (DPF)](https://github.com/DISTRHO/DPF),
which generates the CLAP around our DSP + UI subclasses. The UI is a row of NanoVG-drawn
rotary knobs — **Gain + the four ADSR controls** — that edit the envelope live. Passes
`clap-validator` (18/18).

Next: **Phase 3** drops in the IFS modules (IFSOsc/IFSEnv/IFSLFO) and a 24 dB/oct LPF (pure
module additions, no architectural change), and the UI grows to match. Full plan in
`~/dev-clap/polygraph-clap-plan.md`.

## Layout

The engine (`src/`) mirrors PolygraphModular's structure; the DPF plugin assembly lives in
`plugin/` + `src/UI/`:

```
plugin/               the DPF plugin definition (the CLAP wrapper is generated from these)
                        DistrhoPluginInfo.h (plugin flags), IFSynthParams.h (shared param
                        table), IFSynthPlugin.cpp (DSP: MIDI + params → VoiceManager)
src/UI/               the plugin GUI (reusable NanoVG widgets + the panel)
                        Knob.hpp — labelled rotary-knob widget (NanoSubWidget + KnobEventHandler);
                        GroupBox.hpp — titled container panel that frames + lays out a group of
                        controls; IFSynthUI.cpp — composes the knobs into labelled groups
src/Common/           shim + MIDI + params + utilities
                        pm_compat.h (Apple→portable), pm_midi.* (spec UMP + MIDI-1 decode),
                        clap_ump.h (CLAP↔UMP), pm_params.h / pm_param_data.* / pm_addresses.h,
                        ParamAddressMacros.h, CPPTypes.hpp, CPPProcs.hpp, KPBuffPairHelper.hpp
src/Module/           Module core + the six v1 modules
                        Module.hpp, ModuleFactory.*, SinOscillatorModule.*, ADSR{Helper,Module}.*,
                        CombineModule.*, FinalModule.*, SlewModule.*, PitchConverterModule.*
src/Voice/            voice engine
                        Voice.*, VoiceStateManager.*, VoiceManager.*, ModuleMap.*, ModuleGraph.hpp,
                        ModuleInitSpecs.hpp, pm_module_specs.cpp, ConnectionCommandQueue.hpp
test/                 in-process self-tests (see TESTING.md)
dpf/                  DISTRHO Plugin Framework (submodule, + nested pugl) — generates the .clap
external/clap         CLAP headers (submodule, pinned 1.2.9) — used by the framework self-tests
```

Vendored PolygraphModular files keep their original `#include "X.hpp"` form; every framework
folder is on the include path so they resolve across `Common/` `Module/` `Voice/`. The DSP
wrapper links the `pm_framework` static lib; DPF owns the plugin/UI lifecycle.

## Build

DPF's OpenGL/NanoVG UI needs the usual X11 + GL dev packages (Linux):

```sh
sudo apt install pkg-config libgl1-mesa-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev
```

Then:

```sh
git clone --recurse-submodules <this-repo>
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure     # see TESTING.md for the full suite
```

Produces `build/bin/IFSynth.clap`. Copy/symlink into `~/.clap` (Linux) or
`~/Library/Audio/Plug-Ins/CLAP/` (macOS), load in any CLAP host, and play — it's polyphonic
(8 voices by default; drop the **Gain** knob if chords clip) with a rotary-knob UI for gain
and the ADSR envelope. Validate with
[`clap-validator`](https://github.com/free-audio/clap-validator). Test details in
[TESTING.md](TESTING.md).

## License

MIT.
