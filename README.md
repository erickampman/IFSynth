# IFSynth

A cross-platform **CLAP fractal synth** from Unlikelyware — oscillator, envelopes and LFOs are
all shaped by IFS (Iterated Function System) fractals, drawn and edited directly on custom
fractal-editor controls. Built on the vendored PolygraphModular voice engine and a DPF/NanoVG UI
kit, forked from SimpleSynth, an unreleased Unlikelyware base (SimpleSynth
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
- **Modulation**: source select (LFO 1/2, Env 1/2, CV Mix 1/2, Pressure, Timbre) + bipolar depth
  for IFS Osc pitch / Bx / By / Squash 1 / Squash 2, LPF cutoff and Osc 2 pitch.
- **Voices**: 10-, 20- or 32-voice poly, or mono (legato note-stack, glide, click-free phase continuation).
- **Velocity**: how much note velocity sets loudness, as the VCA's third factor: Off (default;
  every note at full level, as before) / Light / Medium / Full (velocity squared: velocity 64 is
  -12 dB). Latched at note-on; a mono legato note ramps to its new level instead of stepping.
- **Presets**: factory presets in `Presets/*.json`, baked into the plugin at build time; in-UI
  help page (`Resources/help.html`).

MIDI notes are sample-accurate: `process()` splits rendering at each event's frame offset.
Parameter changes from the host are applied at their event; edits from the editor are applied at
the top of the next block.

The envelope Legato and Scale Release params are exposed to the host but don't yet have
custom-UI controls.

The plugin implements the CLAP entry point itself (`src/plugin.cpp`) and keeps the
[DISTRHO Plugin Framework (DPF)](https://github.com/DISTRHO/DPF) purely as the GUI layer, so it can
receive MIDI 2.0 7.9-pitch notes that DPF's generated CLAP wrapper drops.

**MIDI 2.0.** MIDI 1.0 values are upscaled with the spec's Min-Center-Max rule (M2-104 D.1.3), so
127 / 16383 are full scale and 64 / 8192 the exact centre; a MIDI 2.0 note-on with velocity 0 is a
note-on (M2-104 4.2.8), not a note-off. Notes match on note + channel + group, so the same key on two channels is two
voices, and per-note messages reach only their own note. Pitch bend has two parts, summed:

- **Channel bend**, ±2 semitones (MIDI 1.0 or 2.0).
- **Per-note bend** (MIDI 2.0), the channel's range (±2) unless the controller announces its own
  with registered controller 7, which is kept per channel. (M2-104 gives per-note bend no range
  of its own: it "acts like Pitch Bend in every way".)
- **Pitch 7.25** (MIDI 2.0 registered per-note controller #3): an absolute pitch for the sounding
  note, exact at any size; both bends offset from it. It lasts for that note only (no tuning
  table). KeyboardEDO sends it in glide mode.

**MPE** (lower zone), switched on in the Amp box; off by default, and presets leave it alone since
it describes the controller, not the sound. Member channels 2–16 bend ±48, and the master channel's
bend (±2) adds to every member note. **RPN 0** (MIDI 1.0 CC 101/100/6/38, or the MIDI 2.0 registered
controller) sets a channel's bend range; under MPE, any member sets all members. Ranges reset on the
MPE Configuration Message and when the switch flips, and like RC 7 they survive `reset()`.
Once a note is released (note-off, or held only by the pedal), "control of a note ceases"
(MPE §3.3.3): under MPE its member channel's bend is frozen while the tail rings, though the master
channel's still applies, and a released note ignores MIDI 2.0 per-note messages addressed by key.
A CLAP note named by its id keeps taking expression until NOTE_END. With MPE off, a pitch wheel
still bends releasing notes.

**Pressure and Timbre** are modulation sources in every source dropdown. **Pressure** is the
channel pressure of the note's own channel, overridden for that note by polyphonic key pressure
(MIDI 2.0, MIDI 1.0 0xA0, or CLAP's Pressure expression). **Timbre** is CC74 on the note's own
channel, overridden for that note by MIDI 2.0 per-note controller #74 (CLAP's Brightness arrives as
it). Both reset to the channel's value on each note-on and are de-zippered. Under MPE a member note
takes the higher of its own value and the master channel's, and once released it keeps the
member-channel value it had, as the bend does.

**All Notes Off (CC 123)** releases the notes of the channel it arrives on, pedal-held ones included;
they go into their release as after a note-off. **All Sound Off (CC 120)** silences that channel's
voices at once, release included, without touching other channels' bend memory.

The bend goes into both oscillators' second pitch input, after the glide, so glide never smears a
bend; on a mono legato note it ramps rather than jumps. The filter's key tracking follows the note, not the bend.

**CLAP.** Note ids are kept: when a host supplies them (Bitwig does), they decide which note is
which, so a re-struck key is a new note and an expression can name only the id. Note expressions:
**tuning** arrives as Pitch 7.25 at key + tuning, so it is exact at any size, not clamped to the
bend range; **pressure** and **brightness** become polyphonic pressure and per-note controller #74,
which the Pressure and Timbre sources read. A CLAP note gets `CLAP_EVENT_NOTE_END` when its voice
falls silent at the end of the envelope's release (or is stolen, or leaves the mono stack), not at
note-off, so Bitwig keeps its per-voice modulation running through the release. The voice-info
extension reports 10, 20 or 32 voices (1 in Mono), capacity 32, and supports overlapping notes on one key.

## Installing

The download holds `IFSynth.clap`. Put it in your CLAP folder (create the folder if it isn't there),
then rescan plugins in your host:

| | Linux | macOS |
|---|---|---|
| For you | `~/.clap` | `~/Library/Audio/Plug-Ins/CLAP` |
| For every user | `/usr/lib/clap` | `/Library/Audio/Plug-Ins/CLAP` |

On macOS, `~/Library` is hidden in Finder: Go ▸ Go to Folder opens it. The 0.9 previews aren't
notarized, so macOS won't load a downloaded plugin until its quarantine flag is cleared:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/CLAP/IFSynth.clap
```

A host that is already running keeps the plugins it loaded: quit and restart it. To build IFSynth
yourself instead, see below.

## Building from source

```bash
git clone https://github.com/erickampman/IFSynth.git
cd IFSynth
tools/fetch_deps.sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

[BUILDING.md](BUILDING.md) has what to install first on Linux and macOS, the tests, installing
the plugin for hosts, and the build options.

## License

MIT.
