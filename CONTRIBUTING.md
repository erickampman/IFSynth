# Contributing to IFSynth

Bug reports and pull requests are welcome. Build steps are in the README; this page is the rules the
code depends on.

## Before each commit

```sh
ctest --test-dir build --output-on-failure
clap-validator validate build/IFSynth.clap
```

The tests are the `pm-*` engine tests and `ifs-clap`. `ifs-clap` loads the built `.clap` and plays it headless, sending MIDI 2.0, MPE and CLAP events itself, so it needs no MIDI hardware. They are silent. On macOS, if you touched the AUv2 build, also run
`ninja -C build install-auv2` and `auval -v aumu IFSy nLKL`.

After rebuilding, quit and restart the host before listening: a running host keeps the plugin binary
it loaded first.

## Parameters: append, never insert

The CLAP parameter id is the parameter's index, and hosts save sessions and automation by id. So a
new parameter goes at the **end** of the list, and existing ones are never reordered or removed: that
would silently rewire everyone's saved projects. For each new parameter:
- add a `static_assert(symbolIs(...))` line for it in `plugin/IFSynthParams.h`;
- if it shapes the sound, give it a preset key (`src/UI/PresetKeys.hpp`) and add a line for it to every `Presets/*.json`, edited as text so the files aren't reformatted.

MIDI controller setup (MPE on/off and the like) stays out of presets.

## MIDI 2.0 and MPE

MIDI 2.0 per-note messages and CLAP note expressions are the main path. MPE is accepted as a
compatibility input (an Off/On switch, lower zone); nothing in this family of projects sends MPE.

## Threads

`state_load` runs on the main thread while `process()` may be running. It stores the loaded values
only; the audio thread applies them at the start of the next block (or `activate()` does). Never
rebuild the voice graph from the main thread.

## Dependencies

The submodules are pinned (`tools/fetch_deps.sh`). Don't commit changes inside them. A fix needed in
clap-wrapper goes in `patches/` as a patch file, which `fetch_deps.sh` applies, so `external/clap-wrapper`
showing a local change is expected. DPF and pugl are Unlikelyware's forks; changes go to those repos
first and come here as a new pin.

## Drawing (NanoVG)

NanoVG fills non-convex paths through the stencil buffer and assumes it starts at zero, but DPF clears
only colour and depth. On Linux a stale stencil shows as speckles. Any widget that fills a non-convex
shape (a filled waveform, a polygon) must set `glStencilMask(0xff)` and clear the stencil before
drawing. No widget here fills a non-convex shape today. Convex fills (rectangles, rounded rectangles, circles) and strokes are
unaffected.

## Rules learned the hard way

- **Panels before their controls.** Declare each `GroupBox` member before the controls it parents.
  Members are destroyed in reverse order, so a panel freed first crashes its children's destructors
  when the editor closes. The host test only calls the GUI's `create()`, so only a real host catches
  this.
- **Every sound parameter in every preset.** The preset loader leaves keys a preset doesn't mention
  alone, so a missing key keeps the previous preset's value.
- **Output events stay in time order** within a block.
- **Controller setup survives `reset()`.** Configuration a controller sends once (RPN 0, registered
  controller 7) must outlive a reset: some hosts reset on transport stop.
- **Test helpers:** a full-scale bend computes 2³², which wraps to 0 in a `uint32_t`; clamp it.
- **Pitch tests:** measure on the simplest waveform you can. Zero crossings with interpolation are
  exact; autocorrelation needs peak interpolation.

## Platform-specific code

- `src/UI/BackingScale_mac.mm` (Retina scale) and the AUv2 target are macOS-only.
- `uiIdle()` nudges the editor size once on first idle, for Linux hosts (REAPER) that apply the initial size late. It's Linux-only because on a Retina Mac it halves the editor.
