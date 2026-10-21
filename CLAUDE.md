# IFSynth

A CLAP fractal synth: IFS oscillator, envelopes and LFOs on the vendored PolygraphModular engine
(`src/Common`, `src/Module`, `src/Voice`), a hand-written CLAP entry point (`src/plugin.cpp`), and DPF
used only for the GUI (`src/UI/`). Start with README.md and TESTING.md.

Sibling synths, on the same engine and the same raw-CLAP layout. Paths: `~/Dev-Audio/<name>` on the Mac,
`~/dev/dev-clap/<name>` on the Linux PC (as `docs/BuildNotes.md` in IFSynth and StruckSynth shows):
- **StruckSynth** (`~/Dev-Audio/StruckSynth`, branch `main`)
- **PlainOldSynth** (`~/Dev-Audio/PlainOldSynth`, branch `main`), where most MIDI 2.0 work starts

## Working with the author

- Commit to **`master`** directly (this repo is `master`; the siblings are `main`). Never open pull
  requests. Ask before pushing.
- Ask before anything UI-visible: new parameters, panel controls, dropdown entries.
- Parameters are **appended**, never inserted: CLAP parameter ids are the indexes. Each new one needs a
  `static_assert(symbolIs(...))` line in `plugin/IFSynthParams.h`. Sound parameters get a preset key
  (`src/UI/PresetKeys.hpp`) and a line in every `Presets/*.json`, added as text so the files are not
  reformatted; controller setup (MPE) stays out of presets.
- Before each commit: `ctest --test-dir build` and `clap-validator validate build/IFSynth.clap`.
- After a rebuild the author must **quit and restart the host** (StageHand, REAPER, Bitwig): a running
  host keeps the binary it first loaded. Say so when asking them to listen.
- The author checks panel changes by eye; build, then ask, then commit.

## Cross-project policy

- **MPE**: the synths accept MPE as a compatibility input (an Off/On switch, lower zone) and never send
  it. Nothing in StageHand, Umpire or KeyboardEDO originates MPE. MIDI 2.0 per-note messages and CLAP
  note expressions are the preferred path.
- MIDI 2.0 spec on the Mac: `~/Documents/MIDI/MIDI_2-0_Specification_Version_1-0 2/` (M2-104 is v1.0,
  Feb 2020; registered controller 7 is from a later revision and not in it).

## Status (2026-10-17)

The MIDI 2.0 / MPE / CLAP port from the siblings is done (M2 in MILESTONES.md); next is M3.
[docs/MIDI2-PORT.md](docs/MIDI2-PORT.md)
has the checklist, the commit for each row, and the lessons learned.

## On Linux

The Mac is where this was developed; on Linux only CI has built it. Its ubuntu/gcc job failed from the
raw-CLAP commit (`9cab332`) until `e5a32e3`, which builds every static library with `-fPIC`. Since then
CI is green on ubuntu gcc and clang, including the `ifs-clap` host test, which loads the built `.clap`
and plays it headless; so the plugin loads and sounds right on Linux, but nobody has seen its editor.

A checkout from before 2026-10-12 is far behind. To catch up:

1. `git pull` and `tools/fetch_deps.sh` (pinned submodules, only the four VST3 SDK submodules the
   build needs, and the clap-wrapper patch in `patches/`).
2. **Configure a fresh build directory** (`rm -rf build`). The move off `dpf_add_plugin` renamed the
   targets (`IFSynth-clap` → `IFSynth`) and moved the output from `build/bin/IFSynth.clap` to
   `build/IFSynth.clap`. A post-build step re-points `~/.clap/IFSynth.clap` at it.
3. Packages: README's Build section (X11 and GL development headers). If configure or link asks for
   more, PlainOldSynth's README lists the full set its Linux CI installs.
4. `ctest --test-dir build --output-on-failure`, then `clap-validator validate build/IFSynth.clap`.

Never checked on Linux, so check these by hand and record what you find in this file:
- The editor under X11 through the raw CLAP gui extension (`CLAP_WINDOW_API_X11` in
  `src/UI/IFSynthUI.cpp`): correct size on load, resizing, clicks landing on the right controls. The
  first-idle `setSize()` nudge in `uiIdle()` is for Linux hosts (REAPER) that apply the initial size
  late; it is Linux-only because it halves the editor on a Retina Mac.
- The Help button's web view, the Presets menu, and the logo link.
- The VST3 (`build/Release/IFSynth.vst3`, `ninja -C build install-vst3` links it into `~/.vst3`):
  loads, draws, plays in REAPER and Bitwig.
- Sound and MIDI in **StageHand** (`~/dev/dev-audio/StageHand`; Linux-first: JACK through PipeWire, the
  ALSA sequencer) and REAPER.

macOS-only pieces: `src/UI/BackingScale_mac.mm` (Retina scale; built only on Apple).

**NanoVG stencil (seen on Linux, 2026-10-16).** NanoVG fills non-convex paths through the stencil
buffer and assumes it starts at zero, but DPF's `puglOnDisplayPrepare` clears only colour and depth.
On Linux the stale stencil showed through as a speckled pattern across PlainOldSynth's envelope
graphs (fixed there in `bfaa5e5`, `4ea736c`: `EnvelopeGraph::onNanoDisplay` sets `glStencilMask(0xff)`
and clears the stencil before drawing); ModicumFM hit the same thing (`5d36246`, cleared in its
top-level `onNanoDisplay`). This synth has no non-convex fills today, so it is unaffected, but any
new widget that fills a non-convex shape (a filled waveform, a polygon) needs the same clear. Convex
fills (rects, rounded rects, circles, triangles) and plain strokes don't use the stencil.

Test MIDI sources: mpe-sender (`~/Dev-MIDI/mpe-sender` on the Mac, `~/dev/dev-midi/mpe-sender` on the
Linux PC) plays scripted MPE tests on both (CoreMIDI, or the ALSA sequencer as `mpe-sender:MPE Sender`),
and KeyboardEDO sends MIDI 2.0 on both. Also a hardware controller, StageHand, or the headless
`ifs-clap` host test, which sends MIDI 2.0, MPE and CLAP events itself.
