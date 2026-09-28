# IFSynth

A cross-platform **CLAP fractal synth** from Unlikelyware — oscillator, envelopes and LFOs are
all shaped by IFS (Iterated Function System) fractals, drawn and edited directly on custom
fractal-editor controls. Built on the vendored PolygraphModular voice engine and a DPF/NanoVG UI
kit, forked from the [SimpleSynth](https://github.com/erickampman/SimpleSynth) base (SimpleSynth
stays IFS-free; the fractal work lives here).

## Status

**Polyphonic fractal synth with a full custom GUI.** The PM framework drives a `VoiceManager`
(polyphony, note-stealing, mono/legato) rendering, per voice:

```
note pitch → Slew (glide) → Osc 2, IFS Osc, LPF key track

Osc 2 (Sine/Tri/Saw) → Level ─┐
        │ hard sync           ├→ Mix (+) → LPF → VCA (× Env 1 or 2) → Final
        ▼                     │
IFS Osc (× IFS level) ────────┘
```

- **IFS Osc**: 2-transform fractal wavetable, band-limited (mip pyramid shared across voices),
  stereo detune, hard sync to Osc 2 (Off / Up / Down / Both).
- **Osc 2**: band-limited Sine / Triangle / Saw, interval pitch, fine + detune; the hard-sync
  master.
- **LPF**: 24 dB/oct Butterworth with resonance, key tracking and cutoff modulation.
- **Envelopes (×2)**: 3-transform IFS envelopes with hold mode (None / Blue / Green / Loop),
  legato mode (Steal / Match Y, mono only) and Scale Release. Either one drives the VCA.
- **LFOs (×2)** and **CV mixers (×2)**: each mixer combines an LFO, an envelope and a MIDI CC
  (add / multiply).
- **Modulation**: source select (LFO 1/2, Env 1/2, CV Mix 1/2) + bipolar depth for IFS Osc
  pitch / Bx / By / Squash 1 / Squash 2, LPF cutoff and Osc 2 pitch.
- **Voices**: 10-voice poly or mono (legato note-stack, glide, click-free phase continuation).
- **Presets**: factory presets in `Presets/*.json`, baked into the plugin at build time; in-UI
  help page (`Resources/help.html`).

MIDI notes are sample-accurate: `run()` splits rendering at each event's frame offset.
Parameter changes are applied per host block (DPF applies them before `run()`).

The envelope Legato and Scale Release params are exposed to the host but don't yet have
custom-UI controls.

The wrapper is built with the [DISTRHO Plugin Framework (DPF)](https://github.com/DISTRHO/DPF),
which generates the CLAP around our DSP + UI subclasses.

## Layout

The engine (`src/`) mirrors PolygraphModular's structure; the DPF plugin assembly lives in
`plugin/` + `src/UI/`:

```
plugin/               the DPF plugin definition (the CLAP wrapper is generated from these)
                        DistrhoPluginInfo.h (plugin flags), IFSynthParams.h (shared param
                        table), IFSynthPlugin.cpp (DSP: MIDI + params → VoiceManager, mod routing)
src/UI/               the plugin GUI (reusable NanoVG widgets + the panel)
                        IFSControl / IFSEnvControl (fractal editors, drawn from IFSCore so the
                        graph is exactly what you hear); Knob, Slider, Toggle, Dropdown,
                        PopupMenu, TabBar, GroupBox, TitleText, Logo, HelpButton; PresetKeys /
                        PresetDump (preset menu + dev-only JSON dumper); IFSynthUI.cpp — the panel
src/Common/           shim + MIDI + params + utilities
                        pm_compat.h (Apple→portable), pm_midi.* (spec UMP + MIDI-1 decode),
                        clap_ump.h (CLAP↔UMP), pm_params.h / pm_param_data.* / pm_addresses.h,
                        ParamAddressMacros.h, ResonanceComp.hpp, CPPTypes.hpp, CPPProcs.hpp,
                        KPBuffPairHelper.hpp
src/Module/           Module core + modules
                        IFSCore.* (fractal evaluation, wavetables, mip pyramid), IFSOsc, IFSEnv,
                        IFSLFO, SinOscillator, LPFilter (+ ButterLow4), CVMixer, Combine, Level,
                        Slew, PitchConverter, Final; ModuleSharedState.hpp (cross-voice state);
                        ADSR stays instantiated but unconnected
src/Voice/            voice engine
                        Voice.*, VoiceStateManager.*, VoiceManager.*, ModuleMap.*, ModuleGraph.hpp,
                        ModuleInitSpecs.hpp, pm_module_specs.cpp, ConnectionCommandQueue.hpp
Presets/              factory presets (JSON), compiled into GeneratedPresets.h by
                        tools/gen_presets.py
Resources/            font, logo and help page, embedded into the binary at build time
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
git clone --recurse-submodules https://github.com/erickampman/IFSynth.git
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure     # see TESTING.md for the full suite
```

Produces `build/bin/IFSynth.clap`. A post-build step symlinks it into `~/.clap` (Linux) or
`~/Library/Audio/Plug-Ins/CLAP/` (macOS), so any CLAP host picks up the freshest build after a
rescan. Validate with [`clap-validator`](https://github.com/free-audio/clap-validator). Test
details in [TESTING.md](TESTING.md).

Configure options: `-DIFSYNTH_WERROR=ON` (warnings as errors), `-DIFSYNTH_PRESET_DUMP=ON`
(dev-only DUMP button that writes the current patch as JSON to `$HOME`).

## License

MIT.
