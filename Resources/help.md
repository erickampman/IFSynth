<div style="display: flex; align-items: center; gap: 0.75rem; margin: 1.5rem 0 0.5rem;">
  <a href="https://www.unlikelyware.com" target="_blank" rel="noopener noreferrer">
    <img src="Unlikelyware.png" alt="Unlikelyware" style="width: 56px; height: auto; border-radius: 10px; margin: 0; display: block;">
  </a>
  <h1 style="margin: 0;">IFSynth</h1>
</div>

<p>
  <em>IFSynth is a CLAP and VST3 plugin on macOS and Linux, and an AUv2 plugin on macOS. A fractal (Iterated Function System) synthesizer.</em>
</p>

::: {.callout .callout-note .card}
## On This Page
[Overview](#overview)

**Sound —** [IFS Oscillator](#ifs-oscillator) · [Osc 2](#osc-2) · [Low-Pass Filter](#filter)

**Shapes —** [IFS Envelope](#envelope) · [IFS LFO](#lfo)

**Routing and output —** [CV Mixer](#cv-mixer) · [Modulation](#modulation) · [Amp](#amp) · [Presets](#presets)
:::

::: {.callout .callout-note .card}
## Overview {#overview}
Most of IFSynth's sound sources and modulators share the same idea: a small 2D fractal curve, edited directly by dragging its pad, gets read back as a waveform or a control signal. `Split`, `Junction`, `Ends`, and the `Squash` knobs reshape that curve; `Iterations` controls how many times the fractal transform recurses (more = more detail/harmonics); `Smoothing` low-pass-filters the result so the shape doesn't produce harsh steps.
:::

::: {.callout .callout-note .card}
## IFS Oscillator {#ifs-oscillator}
The primary sound source — a fractal-derived wavetable, redrawn from the pad every cycle.

- **Pitch / Fine / Detune:** Coarse tune (semitones), fine tune (cents), and stereo detune (splits left/right frequency — pairs with `Spread` in the Amp section for a wider stereo image).
- **Smoothing / Iterations:** See the fractal-curve note in the [Overview](#overview).
- **Sync:** Hard-sync mode against Osc 2 (Off / Up / Down / Both) — restarts the IFS curve's read position in sync with Osc 2's cycle for classic sync tones.
:::

::: {.callout .callout-note .card}
## Osc 2 {#osc-2}
A conventional band-limited oscillator (Sine / Triangle / Saw) that can act as the hard-sync master for the IFS Oscillator, or just as a second sound source in its own right.

- **Wave:** Sine, Triangle, or Saw, band-limited to avoid aliasing.
- **Pitch:** An interval selector (unison, octaves, fifths, etc.) rather than continuous tuning — meant for locking Osc 2 to a musical interval above/below the IFS Oscillator.
- **Fine / Detune:** Fine tune (cents) and stereo detune, same idea as the IFS Oscillator's.
:::

::: {.callout .callout-note .card}
## Low-Pass Filter {#filter}
A 24 dB/octave Butterworth filter placed after the oscillators, before the amp stage.

- **Cutoff / Resonance:** Standard filter cutoff frequency and resonance (Q).
- **Key Track:** How much the cutoff follows the played note's pitch (0 = fixed cutoff regardless of note, 1 = cutoff tracks pitch 1:1).
- **LPF Modulation:** A dedicated source + depth pair for modulating the cutoff — pick a source (an LFO, an Envelope, a CV Mixer, Pressure or Timbre) in the Modulation panel and set how far it pushes the cutoff.
:::

::: {.callout .callout-note .card}
## IFS Envelope {#envelope}
Two independent fractal envelopes (the `Env 1`/`Env 2` tabs). Envelope 1 normally drives amplitude via the Amp section's VCA source; either can also be routed as a modulation source elsewhere.

- **Duration:** Total envelope time.
- **Blue X/Y, Green X/Y, Squash 1–3:** Shape the fractal envelope curve — the two colored handles on the pad are Blue and Green; dragging them reshapes the attack/decay contour directly.
- **Hold:** Sustain behavior: None (envelope always runs to completion), or hold at the Green/Blue point, or Loop the shape for as long as the note is held.
- **Smoothing / Iterations:** See the fractal-curve note in the [Overview](#overview).
:::

::: {.callout .callout-note .card}
## IFS LFO {#lfo}
Two independent fractal LFOs (the `LFO 1`/`LFO 2` tabs, beside the envelopes), free-running modulation sources for the CV Mixers and Modulation panels.

- **Rate:** LFO speed in Hz.
- **Split, Junction, Ends, Squash 1/2, Smoothing, Iterations:** Same fractal-curve shaping as the Oscillator and Envelope pads, applied to the LFO's waveform instead.
:::

::: {.callout .callout-note .card}
## CV Mixer {#cv-mixer}
Two mixers (side by side, `CV Mixer 1` and `CV Mixer 2`) that combine an LFO, an Envelope, and an incoming MIDI CC into a single modulation source, available anywhere a modulation source can be picked (Modulation panels, LPF cutoff mod).

- **LFO / Env / CC checkboxes:** Enable whichever inputs should feed this mixer.
- **CC #:** Which MIDI CC number to listen to, if the CC input is enabled.
- **Mode:** How the enabled inputs combine: Add (sum them) or Multiply (ring-modulate them together).
:::

::: {.callout .callout-note .card}
## Modulation (IFS Osc / Osc 2 / LPF panels) {#modulation}
Each modulation panel is a set of source-dropdown + depth-knob pairs — pick a source (an LFO, an Envelope, a CV Mixer, Pressure, Timbre, or None) for a given destination, and set how strongly it affects that destination. Depth is bipolar, so negative values invert the modulation.

**Pressure** is aftertouch: the channel's, or the note's own when the controller sends polyphonic pressure. **Timbre** is CC74 (MPE's slide), or the note's own per-note value from a MIDI 2.0 controller or a CLAP host's Brightness.
:::

::: {.callout .callout-note .card}
## Amp {#amp}
- **VCA env:** Which envelope (1 or 2) drives overall amplitude.
- **Voices:** Mono (one voice, with legato note-stacking and Glide) or Poly (up to 10 simultaneous voices, each independent).
- **MPE:** On: read MPE input (lower zone). Member channels 2–16 bend ±48 semitones, and channel 1's bend adds to every note. Describes your controller, not the sound, so presets leave it alone.
- **Velocity:** How much note velocity sets loudness: Off (every note at full level), Light, Medium, or Full (velocity squared — a half-strength note is 12 dB quieter).
- **Osc 2 / IFS:** Relative output level of each oscillator in the final mix.
- **Spread:** Stereo width of the summed output (0% = mono, 100% = full width) — pairs with the oscillators' Detune controls: at 0% spread, detuned left/right signals fold back together and the detune shimmer disappears; at 100% you get the full stereo effect.
- **Glide:** Portamento time between notes. Only active in Mono voice mode — in Poly mode each voice starts fresh, so there's no previous pitch to glide from.
:::

::: {.callout .callout-note .card}
## Presets {#presets}
The dropdown next to the logo recalls a saved patch, applying every parameter it specifies at once. Parameters a preset doesn't mention are left at their current value.

**Save** and **Load**, left of the Help button, write the patch to a file and read one back, through the system's file dialog. The first time, the dialog opens on **Documents/IFSynth**; after that, on the last folder you used.

- A saved patch has every sound parameter and leaves out MPE; loading one sets those parameters and leaves MPE alone.
- The preset menu then shows the patch's name, dimmed.
- The file is the same JSON as the factory presets, so a patch you save can be put in the source's `Presets` folder to build it in as a factory preset.
- The host still saves the patch with its project, whether or not you saved it to a file.
:::
