<div style="display: flex; align-items: center; gap: 0.75rem; margin: 1.5rem 0 0.5rem;">
  <a href="https://www.unlikelyware.com" target="_blank" rel="noopener noreferrer">
    <img src="Unlikelyware.png" alt="Unlikelyware" style="width: 56px; height: auto; border-radius: 10px; margin: 0; display: block;">
  </a>
  <h1 style="margin: 0;">IFSynth</h1>
</div>

<p>
  <em>IFSynth is a CLAP and VST® 3 plugin on macOS and Linux, and an AUv2 plugin on macOS. A fractal (Iterated Function System) synthesizer.</em>
</p>

::: {.callout .callout-note .card}
## On This Page
[Overview](#overview) · [The Window](#window) · [Using the Controls](#controls)

**Sound —** [IFS Oscillator](#ifs-oscillator) · [Osc 2](#osc-2) · [Low-Pass Filter](#filter)

**Shapes —** [IFS Envelope](#envelope) · [IFS LFO](#lfo)

**Routing and output —** [CV Mixer](#cv-mixer) · [Modulation](#modulation) · [Amp](#amp) · [Presets](#presets)
:::

::: {.callout .callout-note .card}
## Overview {#overview}
Most of IFSynth's sound sources and modulators share the same idea: a small 2D fractal curve, edited directly by dragging on its pad, gets read back as a waveform or a control signal. On the pad, `Split`, `Junction`, `Ends`, and the `Squash` regions reshape that curve; the `Iterations` dropdown sets how many times the fractal transform recurses (more = more detail/harmonics); the `Smoothing` knob low-pass-filters the result so the shape doesn't produce harsh steps. The curve drawn on each pad is the exact shape the synth plays, smoothing and iterations included.
:::

::: {.callout .callout-note .card}
## The Window {#window}
Along the top: the Unlikelyware logo, the **IFSynth** wordmark, and the **Preset** menu; at the far right, **Save**, **Load**, and **Help**. **Help** opens this page in a panel inside the plugin window, covering everything below the top strip; click **Help** again to close it.

Below that, four rows of panels:

| Row | Left | Right |
|---|---|---|
| 1 | [IFS Oscillator](#ifs-oscillator) | [Osc 2](#osc-2) |
| 2 | [IFS Envelope / IFS LFO](#envelope) (tabs `Env 1` `Env 2` `LFO 1` `LFO 2`) | [Low-Pass Filter](#filter) |
| 3 | [CV Mixer 1](#cv-mixer) · [CV Mixer 2](#cv-mixer) | [Amp](#amp) |
| 4 | [IFS Osc Modulation](#modulation) | [Osc 2 Mod](#modulation) · [LPF Modulation](#modulation) |

The window can be resized from the host; it keeps its proportions, and everything scales with it.
:::

::: {.callout .callout-note .card}
## Using the Controls {#controls}
- **Knobs:** Drag up to increase, down to decrease. Hold **Ctrl** while dragging (or scrolling) for finer steps. The scroll wheel also turns a knob. **Shift**-click returns a knob to its default.
- **Dropdowns:** Click to open the menu, then click a choice. Clicking outside the menu, pressing **Escape**, or switching to another window closes it without changing anything.
- **Checkboxes and tabs:** Click to flip or select.
- **Pads:** Drag the dots and regions directly; each pad's section below says what each part does.
:::

::: {.callout .callout-note .card}
## IFS Oscillator {#ifs-oscillator}
The primary sound source — a fractal-derived wavetable, drawn by the pad on the left of the panel.

- **Pad:** Drag the middle dot to move the `Split` (left/right) and the `Junction` (up/down). Drag either end dot up or down to set `Ends` — the two ends move together, so the wave always joins up. Drag up or down anywhere else left or right of the split to set `Squash 1` or `Squash 2` for that region.
- **Osc Pitch / Osc Fine / Detune:** Coarse tune (±24 semitones), fine tune (±100 cents), and stereo detune (0–25 cents, splitting left from right — pairs with `Spread` in the Amp section for a wider stereo image).
- **Smoothing:** See the fractal-curve note in the [Overview](#overview).
- **Iterations:** 3 or 4 (default 4).
- **Sync:** Hard-sync to Osc 2: Off, Up, Down, or Both. When on, the IFS Oscillator restarts its cycle each time Osc 2 crosses zero — rising (Up), falling (Down), or either way (Both) — for classic sync tones. Left and right sync separately, so Osc 2's Detune carries through to the synced sound.
:::

::: {.callout .callout-note .card}
## Osc 2 {#osc-2}
A conventional band-limited oscillator that can act as the hard-sync master for the IFS Oscillator, or just as a second sound source in its own right. Its level is the `Osc 2` knob in the [Amp](#amp) section, which starts at zero, so turn that up to hear it.

- **Wave:** Sine, Triangle, or Saw, band-limited to avoid aliasing.
- **Pitch:** An interval rather than continuous tuning, for locking Osc 2 to a musical interval below or above the IFS Oscillator: -3 oct, -2 oct, -1 oct, 0 (unison, the default), +5th, +1 oct, or +1 oct +5th.
- **Fine / Detune:** Fine tune (±100 cents) and stereo detune (0–25 cents), same idea as the IFS Oscillator's.
:::

::: {.callout .callout-note .card}
## Low-Pass Filter {#filter}
A 24 dB/octave Butterworth filter placed after the oscillators, before the amp stage.

- **Cutoff:** About 7 Hz to 18.5 kHz (default 14 kHz).
- **Resonance:** Filter Q, 1 to 20 (default 1).
- **Key Track:** How much the cutoff follows the played note's pitch (0 = fixed cutoff regardless of note, the default; 1 = cutoff tracks pitch 1:1).

The cutoff's modulation is in its own **LPF Modulation** panel at the bottom right — see [Modulation](#modulation).
:::

::: {.callout .callout-note .card}
## IFS Envelope {#envelope}
Two independent fractal envelopes, `Env 1` and `Env 2`. They share one panel with the two LFOs: the four tabs at the top right of the panel (`Env 1`, `Env 2`, `LFO 1`, `LFO 2`) pick which one is shown. The Amp section's `VCA env` picks which envelope drives amplitude (Env 1 by default); either can also be routed as a modulation source elsewhere.

- **Pad:** The envelope always starts and ends at zero. Drag the Blue and Green dots (Blue stays left of Green) to reshape the attack/decay contour; drag up or down anywhere else to set `Squash 1`, `Squash 2`, or `Squash 3` for the region left of Blue, between the dots, or right of Green.
- **Duration:** Total envelope time, 10 ms to 10 s (default 1 s).
- **Smoothing:** See the fractal-curve note in the [Overview](#overview).
- **Hold:** Sustain behavior: None (the envelope always runs to completion), Blue or Green (hold at that dot while the note is held, the default being Green), or Loop (repeat the shape for as long as the note is held).
- **Legato:** What a new note does to an envelope that is still running, in Mono voice mode only: Steal (restart from the top) or Match Y (the default: jump to the point in the shape at the level the envelope is already at, so the retrigger doesn't click). In Poly, every note starts from the top.
- **Iterations:** 1 to 8 (default 3).
- **Scale Rel:** Checkbox, off by default. With Hold set to Blue or Green, a note released before the envelope reaches the hold point jumps to the hold point and plays the rest of the shape scaled to start from the current level, instead of running the full cycle.
:::

::: {.callout .callout-note .card}
## IFS LFO {#lfo}
Two independent fractal LFOs, `LFO 1` and `LFO 2`, shown by the `LFO 1` and `LFO 2` tabs of the envelope panel. They are free-running modulation sources for the CV Mixers and Modulation panels.

- **Pad:** Edited exactly like the [IFS Oscillator's](#ifs-oscillator) pad — `Split`, `Junction`, `Ends`, `Squash 1/2` — applied to the LFO's waveform instead.
- **Rate:** LFO speed, about 0.008 Hz to 128 Hz (default 1 Hz).
- **Smoothing:** See the fractal-curve note in the [Overview](#overview).
- **Iterations:** 1 to 8 (default 4).
:::

::: {.callout .callout-note .card}
## CV Mixer {#cv-mixer}
Two mixers (side by side, `CV Mixer 1` and `CV Mixer 2`) that each combine an LFO, an Envelope, and an incoming MIDI CC into a single modulation source, available anywhere a modulation source can be picked (as `CV Mix 1` and `CV Mix 2`). CV Mixer 1 takes LFO 1 and Env 1; CV Mixer 2 takes LFO 2 and Env 2.

- **LFO 1 / Env 1 / CC checkboxes** (LFO 2 / Env 2 / CC on Mixer 2): Enable whichever inputs should feed this mixer. The LFO and Env are on by default, CC off.
- **CC #:** Which MIDI CC number to listen to (1–127, default 1, the mod wheel), if the CC input is enabled.
- **Mode:** How the enabled inputs combine: Add (sum them) or Multiply (ring-modulate them together).
:::

::: {.callout .callout-note .card}
## Modulation (IFS Osc / Osc 2 / LPF panels) {#modulation}
The bottom row holds three modulation panels. Each is made of source-dropdown + depth-knob pairs: the dropdown, captioned with its destination, picks a source (None, LFO 1, LFO 2, Env 1, Env 2, CV Mix 1, CV Mix 2, Pressure, or Timbre), and the **Depth** knob under it sets how strongly it affects that destination. Depth is bipolar, so negative values invert the modulation; every depth starts at zero, and every source at None.

- **IFS Osc Modulation:** Five pairs, one each for the IFS Oscillator's **Pitch**, **Split**, **Junction**, **Squash 1**, and **Squash 2**. Depth -1 to +1.
- **Osc 2 Mod:** One pair (**Source**) for Osc 2's pitch — vibrato from a CC through a CV Mixer, say, even while Osc 2 drives sync. Depth -1 to +1.
- **LPF Modulation:** One pair (**Cutoff**) for the filter cutoff. Depth -4 to +4.

**Pressure** is aftertouch: the channel's, or the note's own when the controller sends polyphonic pressure. **Timbre** is CC74 (MPE's slide), or the note's own per-note value from a MIDI 2.0 controller or a CLAP host's Brightness.
:::

::: {.callout .callout-note .card}
## Amp {#amp}
Four dropdowns across the top, four knobs under them.

- **VCA env:** Which envelope (Env 1 or Env 2) drives overall amplitude.
- **Voices:** Mono (one voice, with legato note-stacking and Glide), or Poly 10, Poly 20, or Poly 32 (that many simultaneous voices, each independent). Default Poly 10.
- **MPE:** On: read MPE input (lower zone). Member channels 2–16 bend ±48 semitones, and channel 1's bend adds to every note. Describes your controller, not the sound, so presets leave it alone. Off by default.
- **Velocity:** How much note velocity sets loudness: Off (every note at full level, the default), Light, Medium, or Full (velocity squared — a half-strength note is 12 dB quieter).
- **Osc 2 / IFS:** Output level of each oscillator in the final mix (defaults: Osc 2 at 0, IFS at 0.5).
- **Spread:** Stereo width of the summed output (0% = mono, 100% = full width, the default) — pairs with the oscillators' Detune controls: at 0% spread, detuned left/right signals fold back together and the detune shimmer disappears; at 100% you get the full stereo effect.
- **Glide:** Portamento time between notes, 0.1 ms to 2 s (default 10 ms). Only active in Mono voice mode, and greyed out otherwise — in Poly mode each voice starts fresh, so there's no previous pitch to glide from.
:::

::: {.callout .callout-note .card}
## Presets {#presets}
The **Preset** menu, right of the IFSynth wordmark in the top strip, recalls a factory patch, applying every parameter it specifies at once. Parameters a preset doesn't mention are left at their current value. It shows `--` until you pick one.

**Save** and **Load**, left of the Help button, write the patch to a file and read one back, through the system's file dialog. The first time, the dialog opens on **Documents/IFSynth**; after that, on the last folder you used.

- A saved patch has every sound parameter and leaves out MPE; loading one sets those parameters and leaves MPE alone.
- The preset menu then shows the patch's name, dimmed. If the file couldn't be written or read, it says `Couldn't save` or `Couldn't load` there instead.
- The file is the same JSON as the factory presets, so a patch you save can be put in the source's `Presets` folder to build it in as a factory preset.
- The host still saves the patch with its project, whether or not you saved it to a file.
:::

VST is a registered trademark of Steinberg Media Technologies GmbH.
