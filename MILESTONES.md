# IFSynth milestones

## Tests
- all: cmake --build build -j && ctest --test-dir build --output-on-failure

## M1: Core synth
- [x] PolygraphModular voice engine: VoiceManager, polyphony, mono/legato/glide
- [x] IFS Osc (fractal wavetable, band-limited mip pyramid, stereo detune, hard sync)
- [x] Osc 2 (Sine/Triangle/Saw), IFS Envelopes ×2, LFOs ×2, CV Mixers ×2, LPF
- [x] Modulation routing: source dropdowns, bipolar depth for IFSOsc/LPF/Osc2 targets
- [x] Preset system: factory presets baked in, in-UI help page
- [x] DPF/NanoVG custom UI: fractal editors, knobs, sliders, tabs, dropdowns

## M2: MIDI 2.0 and MPE port
- [x] Raw CLAP entry point (DPF as GUI only, so MIDI 2.0 note dialect is available)
- [x] Per-note UMP decode; note matching on note + channel + group; RC 7 bend range
- [x] Channel and per-note pitch bend summed; Pitch 7.25 glide; MIDI 1.0 Min-Center-Max upscale
- [x] CLAP note ids; note expressions (tuning as Pitch 7.25; pressure/brightness decoded)
- [x] `CLAP_EVENT_NOTE_END`; voice-info extension (10 voices, overlapping notes)
- [x] MPE lower zone: RPN 0 bend range, released-note control freeze (bend), MPE on/off dropdown
- [x] MIDI 2.0 velocity-0 note-on treated as note-on; velocity loudness dropdown
- [x] All Notes Off (CC 123) and All Sound Off (CC 120), per channel
- [x] Pressure and Timbre (CC 74) as modulation sources (add to routing dropdowns)

## M3: Plugin formats and polish
- [x] Plugin id `com.unlikelyware.ifsynth` (was `com.example.ifsynth`; breaks saved CLAP sessions)
- [x] VST3 via clap-wrapper (macOS + Linux); Tuning + Brightness + Pressure note expressions via
      CLAP_PLUGIN_AS_VST3. Steinberg validator: 47 pass, 0 fail (macOS, 2026-10-21)
- [x] AUv2 via clap-wrapper (macOS, `aumu IFSy nLKL`); auval passes; AUv2 GUI patch in patches/
- [ ] By hand: Logic (AUv2), REAPER (VST3 + AUv2), Bitwig (VST3 per-note Tuning); Linux VST3
- [ ] Custom UI controls for Legato and Scale Release params (currently host-only)
- [ ] Pitch 7.25 as persistent per-key tuning table (CLAP tuning stays per-note)
- [ ] Move onto the DPF fork (erickampman/DPF) with own DGL_NAMESPACE
- [ ] Use uw::ump for UMP decode (plugins-roadmap.md)

## Save and Load (2026-10-27)
- [x] Save / Load in the header write the patch as the factory presets' JSON and read it back (src/UI/PresetFile.hpp),
      through DPF's file dialog; MPE left out, loaded values clamped to each param's range.
      Ported from StruckGaussian (ReleaseUMP R2-15). The patch mirror the
      dumper used is now always kept (Save writes it), and set to the defaults before the editor reads the patch, not
      after, which had made a reopened editor dump the defaults.
      ifs-presetfile checks the round trip, every factory preset, and non-presets
- [x] AUv2: clap-wrapper gives no timers and (unpatched) never calls gui show(), so the UI's idle never ran and DPF would never
      collect the dialog's result. The GUI now runs its own main-run-loop timer when the host has none, started at
      set_parent; the home folder comes from the account, not the sandboxed $HOME; failures show in the Preset menu
      (StruckGaussian 3f0c97c)
- [ ] By hand: Save and Load in Logic Pro (AUv2) and a CLAP host on macOS; Windows and Linux (the dialog is native on
      Windows, DPF's own on Linux)
