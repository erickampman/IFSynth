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
- [ ] Pressure and Timbre (CC 74) as modulation sources (add to routing dropdowns)

## M3: Remaining polish
- [ ] Custom UI controls for Legato and Scale Release params (currently host-only)
- [ ] Move onto the DPF fork (erickampman/DPF) with own DGL_NAMESPACE
- [ ] Use uw::ump for UMP decode (plugins-roadmap.md)
