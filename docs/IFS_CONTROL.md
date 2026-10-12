# IFS Waveform Control — design handoff

Session handoff for the custom IFS editor control. Written so a fresh Claude session — on any
machine — can pick up with full context after a `git clone`/`pull`.

**IFSynth is a fork of the [SimpleSynth](https://github.com/erickampman/SimpleSynth) base**
(full history preserved). SimpleSynth stays IFS-free as a reusable base; all the fractal
work — the IFS modules and this control — lives here.

Interactive prototype (view in a browser signed into the same Claude account):
<https://claude.ai/code/artifact/7c8b58cb-6bc9-4b2b-952d-20b15b960c38>

---

## Where the base stands right now (inherited from SimpleSynth)

- A cross-platform **CLAP synth** porting the PolygraphModular (PM) AUv3 voice framework off
  Apple. Polyphonic, real PM voice engine (VoiceManager → per-voice PitchConverter → Slew →
  SinOsc → Combine ×ADSR → Final). Passes `clap-validator` (18/18); 8 framework self-tests.
- Plugin wrapper was **DPF** (generated the `.clap` from a DSP subclass + a UI subclass); the
  `pm_framework` static lib is the reused engine. Output was `build/bin/SimpleSynth.clap`.
  IFSynth now implements the CLAP entry point itself (`src/plugin.cpp`) with DPF kept for the GUI
  only, so it can receive MIDI 2.0 7.9-pitch notes.
- **UI** is a NanoVG panel with a reusable widget kit:
  - `src/UI/Knob.hpp` — labelled rotary knob (NanoSubWidget + DPF KnobEventHandler).
  - `src/UI/GroupBox.hpp` — titled container panel that frames + row-lays-out its child controls.
  - `src/UI/SimpleSynthUI.cpp` — composes them: "ADSR Envelope" panel (A/D/S/R) + "Output"
    panel (Gain).
  - **Convention: each module = one `GroupBox` with its controls.** The IFS control below will
    live in its module's GroupBox.
- Repo: **private** GitHub `erickampman/SimpleSynth`, branch `master`, push-triggered CI
  (Linux gcc/clang + macOS clang). Submodules: `dpf`, `external/clap`.
- **Open decision — license:** MIT (matches netmidi2) vs GPLv3 (share but block closed
  commercial forks of the framework). Leaning GPLv3; no `LICENSE` file yet. Only matters if the
  repo is made public.
- **Next major step: Phase 3** — port the IFS modules (IFSOsc / IFSEnv / IFSLFO) + a 24 dB/oct
  LPF from PM into `pm_framework`, each surfacing as a GroupBox in the UI. The IFS control is
  the centerpiece of those panels.

---

## The IFS control — what it is

A 2-D "fractal shaping" pad that edits one IFS module's transforms directly. X is the curve's
domain (phase for the Osc, time for the Env/LFO), normalized 0–1; Y is the value in [-1, 1].
The **IFS Osc and IFS LFO use 2 transforms** (one split, the prototype's case); the **IFS Env
uses 3 transforms** (two splits — generalize later).

The resultant fractal is drawn in the background; translucent "squash" rectangles overlay it.

### Handles / interactions (2-transform case)

| Handle | Gesture | Param |
|---|---|---|
| **Middle dot** (2-D, at the junction) | drag horizontally = split point; vertically = junction height | `bx`, `by` |
| **End handles** (both edges, at the start/end level) | drag either — both ends move together (continuity) | `sEY` |
| **Left region** | drag up/down anywhere in the region | `s1` (squash) |
| **Right region** | drag up/down anywhere in the region | `s2` (squash) |

Locked decisions:
- The **middle dot is always 2-D** (no X-only mode).
- **Squash is absolute**: the rect's top edge snaps to the cursor's Y; the rect grows from the
  center line, **up = positive, down = negative** (blue below the line for negative), and its
  height *is* the value. Range ±1 (clamp ±0.99, per the reference).
- **No skew** — the curve is a strict function by construction (each region is a normalized
  self-similar copy of the whole).
- Endpoints are **linked** (`sEY` shared by x=0 and x=1).

### Parameters (per 2-transform IFS)

| Param | Range | Meaning |
|---|---|---|
| `bx` | −1…1 → split at `(bx+1)/2`, clamped [0.01, 0.99] | X split between region 1 and 2 |
| `by` | −1…1 | curve value at the junction point B = (split, by) |
| `sEY` (startEndY) | −1…1 | value at both ends |
| `s1` (ifs1Squash) | ±1 | left region self-similar vertical scale |
| `s2` (ifs2Squash) | ±1 | right region self-similar vertical scale |
| `smoothing` | 0…1 | Catmull-Rom (0) ↔ piecewise-linear (1) render blend |
| `iterations` | 3 or 4 | recursion depth |

Each of these must become a real automatable plugin parameter (its own `MODULE_PARAM_ADDRESS`)
— the control is just a 2-D, multi-param *view* over them, exactly like the knobs are 1-D views.
For the 3-transform Env: two splits + one `sEY` + three squash → ~6 shape params.

---

## The fractal math (authoritative)

Ported verbatim from the reference **`IFSOscView.swift`** →
`PolygraphModularExtension/ModularUI/UI/IFSOscView.swift` in the AUApr25 repo (read-only; do
NOT modify AUApr25). It mirrors `IFSCore.cpp`. The recursion:

```
evalIFS(x, depth, bxNorm, by, sEY, s1, s2):
  if depth == 0:                      # base tent: (0,sEY) → (bxNorm,by) → (1,sEY)
     return x <= bxNorm
        ? sEY + (by - sEY) * (x / bxNorm)
        : by  + (sEY - by) * ((x - bxNorm) / (1 - bxNorm))
  if x <= bxNorm:
     xn = x / bxNorm
     return sEY + (by - sEY)*xn + s1 * (evalIFS(xn, depth-1, ...) - sEY)
  else:
     xn = (x - bxNorm) / (1 - bxNorm)
     return by  + (sEY - by)*xn + s2 * (evalIFS(xn, depth-1, ...) - sEY)
```

Optional smoothing runs Catmull-Rom through the IFS vertices (`collectVertexX` gives the
`2^iters + 1` vertex X positions) and blends: `out = smoothed + smooth*(raw - smoothed)`.
The prototype (`ifs_control.html`, in the artifact) has the full JS port of all of this if a
line-by-line reference is handy.

---

## Next step — port the prototype to the plugin

1. Add `src/UI/IFSControl.hpp` — a DPF `NanoSubWidget` alongside `Knob`/`GroupBox`. Canvas 2D →
   NanoVG is ~1:1 (paths, arcs, `globalAlpha` ↔ alpha fills). Reuse the prototype's drawing +
   hit-testing/drag logic.
2. It edits N shape params; expose them as plugin parameters and route drags through a callback
   (mirror `Knob::Callback` → `setParameterValue` / `editParameter`, keyed by `getId()` or a
   small per-handle id). Each underlying param also needs a `MODULE_PARAM_ADDRESS` on the real
   IFS module (comes with Phase 3).
3. Draw the background curve by calling the ported `evalIFS` (the same routine the DSP module
   will use — share it), so the UI curve and the audio agree.
4. Drop the control into the IFS module's `GroupBox`.

Prototype behavior to preserve: middle dot 2-D, absolute squash from the center line, linked
ends, teal(+)/blue(−) translucent squash rects over a phosphor-green curve, iterations + a
smoothing control. Colors are provisional.

Generalize to 3 transforms (Env) by supporting 2 ordered (non-crossing) split dots and 3 squash
regions.
