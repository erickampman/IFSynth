# IFSynth: MIDI 2.0 / MPE / CLAP port

Written 2026-10-11 to start the port; kept up to date as it went (last 2026-10-17). Status: **every
row is done except item 3** (pressure and timbre as modulation sources). The author paused the port
on 2026-10-12 to bring the Linux machine up to date, and resumed it on 2026-10-17; see
[CLAUDE.md](../CLAUDE.md) for how to work here.

The same work was done first in two sibling repos; port from them rather than re-deriving:

- **StruckSynth** (`~/Dev-Audio/StruckSynth`, branch `main`): the closest model, because it went through
  every step below from the same starting point.
- **PlainOldSynth** (`~/Dev-Audio/PlainOldSynth`, branch `main`): where most of it began. It has a
  modulation system like IFSynth's, so for item 3 it is the better model.

All three vendor the same PolygraphModular engine (`src/Common`, `src/Module`, `src/Voice`), so most
engine changes port nearly file for file: `git show <commit> -- src > x.patch` then
`git apply --reject x.patch`, and place the rejected hunks by hand (they are usually context that
differs, such as StruckSynth's ModalBank).

## IFSynth as it is now

- IFSynth commits to **`master`** (not `main`). Remote: `github.com/erickampman/IFSynth`.
- **The voice** (`VoiceManager::buildDefaultPatch`): SinOsc (via a Level) + IFSOsc → Combine(Add) → LPF →
  Combine(×) VCA with IFSEnv 1 (or 2) and Velocity → Final. PitchConverter → Slew → both oscillators'
  pitch inputs and the LPF key tracking. PitchBendConverter → both oscillators' second pitch inputs,
  after the Slew. SinOsc syncs IFSOsc. LFO 1/2 and Env 2 feed two CVMixers (LFO + Env + one CC each).
- **Modulation routing** (`applyModRouting` in `src/plugin.cpp`): dropdowns pick a source (None, LFO 1/2,
  Env 1/2, CV Mix 1/2) for IFSOsc pitch / Bx / By / Squash 1 / Squash 2, LPF cutoff, SinOsc pitch, and
  the VCA envelope.
- **Parameters** are appended, never inserted (CLAP ids are indexes); `plugin/IFSynthParams.h` asserts
  every row's symbol, so a new parameter needs a `static_assert(symbolIs(...))` line there too.

## The checklist, with what is different in IFSynth

Done roughly in the order of the table, one commit each, and update README / TESTING as you go.
The SS # column is StruckSynth's step number, kept for cross-reference; it is not the order.

| SS # | What | Port from | IFSynth notes |
|---|---|---|---|
| 0 | Raw CLAP entry point | already in the tree | **Done** (`9cab332`, plus `191c0c6`, the sync-reset phase wrap left over after upstream fixed the same IFSOsc crash). |
| 6 | `static_assert` that `kParams` rows match `ParamIndex` order | SS `a256ccc`, POS `ad0426c` | **Done** (`d9a1b89`): one assert per row, plus dropdown label counts against ranges. New parameters need a line there. |
| 4 | Retina / multi-monitor GUI sizing | SS `4ed5f93`, POS `d60a932` | **Done** (`5cf88a5`), checked on both monitors. Upstream's first-idle `setSize()` nudge is now Linux-only (it halved the editor on Retina). |
| 5 | Never call `params.rescan` from the audio thread | POS `f201e8c` | **Nothing to do** (checked 2026-10-12). `notifyHostValuesChanged` (`rescan` at line 366) is called only from `state_load` (lines 809, 833), on the main thread. POS's fix was for mod-destination switch-offs changing other parameters; IFSynth's `applyModRouting` changes none. |
| 1 | Per-note UMP decode; notes match on note + channel + group; engine note-offs built from the note-on; pitch bend | SS `586cee7`, POS `793a139` | **Done** (`80c3008`), including RC 7 from SS `586cee7`. Bend feeds a new `IFSOscParamItemPitchMod2Input` and SinOsc's existing `ModPitch2`, after the Slew. LPF key tracking does not follow the bend (a choice, noted in `buildDefaultPatch`; easy to change). |
| 1+ | Per-note bend range = the channel's (±2) unless **registered controller 7** sets it; RPN 0 and RC 7 kept across `reset()`; **Pitch 7.25** (per-note controller #3) | SS `7baf056`, `c32681d`; POS `69b455e` | **Done** (`3967e15`), from SS rather than POS's module (MPE comes in step 7). Mono legato note-ons ramp the bend (`legatoContinuation`). RPN 0 (channel bend range) is not handled yet: it arrives with MPE in step 7. |
| 2 | CLAP note ids; `CLAP_EVENT_NOTE_EXPRESSION`; tuning as Pitch 7.25 | SS `c32681d`, POS `623a64f` + `69b455e` | **Done** (`153a6d0`). Pressure and brightness are decoded but nothing in the voice reads them until step 3. |
| — | `CLAP_EVENT_NOTE_END` + voice-info extension | SS `3cf2f03`, POS `559b80b` | **Done** (`406a452`). GUI param events are pushed at the top of `process()`, before any NOTE_END, so no queueing was needed; the host test checks the order. |
| 7 | MPE switch (lower zone) + RPN 0 | SS `d7258a2`, POS `5dfb576`, `714c808` | **Done** (`de5f654`): an Off/On dropdown in the Amp box beside Voices, kept out of presets. `6e8c43c` made the third row taller so the Amp knobs could grow. |
| — | Released notes (MPE §3.3.3: control ceases at Note Off) | SS `b43e91b`, POS `228facb` | **Done** (`209008b`), bend only. When step 3 adds pressure and timbre, freeze their member-channel values at release too, as POS `228facb` does. |
| 8 | MIDI 1.0 → 2.0 `pm::scaleUp`; a MIDI 2.0 velocity-0 note-on is a note-on | SS `76d53ca`, POS `595fd50` + `3820e90` | **Done** (`2f30d3d`), plus POS's Velocity loudness dropdown (`8eb0d1d`) in the Amp box. |
| — | All Notes Off (CC 123) / All Sound Off (CC 120), per channel | SS `30a1ca6`, POS `a3b94c7` | **Done** (2026-10-17), from POS. All Sound Off resets IFS Env 1/2, the ADSR, IFSOsc and SinOsc, never `Voice::reset()`: that also wipes the per-channel bend and pressure memory. Tests in `pm-voicemanager`. |
| 3 | Pressure and Timbre as modulation sources | POS `a65a645` (+ later fixes) | IFSynth has routing dropdowns, so follow POS: add Pressure and Timbre (CC74 per note) to `kModSourceChoices`, appended so saved choices keep their meaning. Ask the author first; it is UI-visible. IFSynth's CVMixer only reads one CC on any channel. |

Ask the author before building anything UI-visible: new parameters, panel controls, dropdown entries.

## Lessons from StruckSynth and POS

- **Restart the host after every rebuild.** StageHand / REAPER keep the plugin binary they first loaded;
  reopening the plugin is not enough.
- **Append parameters**, never insert: CLAP parameter ids are the indexes. Give new sound parameters
  preset keys and add them to the factory preset JSON (as text, so the files are not reformatted);
  leave controller-setup parameters (MPE) out of presets.
- **Pitch tests:** measure on the simplest waveform you can. Zero crossings with interpolation are
  exact; autocorrelation needs peak interpolation (both repos' host tests once read whole-sample or
  rising-edge lags and printed wrong pitches).
- **Test helpers:** a full-scale bend computes 2³², which wraps to 0 in a `uint32_t`; clamp it.
- **Output events must stay in time order** within a block.
- **Configuration a controller sends once** (RPN 0, RC 7) must survive `reset()`: some hosts reset on
  transport stop.
- **Spec:** `~/Documents/MIDI/MIDI_2-0_Specification_Version_1-0 2/` (M2-104 is v1.0, Feb 2020; RC 7 is
  from a later revision and not in it). No `pdftotext` on this machine; StruckSynth's session extracted
  the text with a small Swift PDFKit script.
- **MPE §3.3.3** ("control of a note ceases once Note Off has occurred") was taken from a forum quote of
  the spec, not the official document; confirm it if the official MPE spec turns up.
- **Listening tests:** `~/Dev-MIDI/mpe-sender`; KeyboardEDO for MIDI 2.0 (it sends RC 7, per-note bend,
  and Pitch 7.25 in glide mode).

## Working with the author

- Commit to `master` directly, no pull requests; ask before pushing.
- Policy across these projects: synths accept MPE as a compatibility input and never send it.
- Run `clap-validator validate build/IFSynth.clap` and the CTest suite before each commit.
