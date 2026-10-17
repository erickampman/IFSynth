# Testing IFSynth

The project ships a suite of in-process self-tests (via CTest) plus the CLAP conformance
validator. Everything runs headless on Linux/macOS — no DAW or audio device required.

## Prerequisites

- CMake ≥ 3.17 and a C++20 compiler (gcc or clang).
- Submodules (CLAP headers for the tests; DPF + pugl for the plugin build):

  ```sh
  git submodule update --init --recursive
  ```

- To build the plugin itself (DPF's OpenGL/NanoVG UI), the X11 + GL dev packages:

  ```sh
  sudo apt install pkg-config libgl1-mesa-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev
  ```

## Build and run all tests

```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Add `-DIFSYNTH_WERROR=ON` at configure time to treat warnings as errors for the framework
and the `pm-selftest` glue (this is what CI keys on). The tests that pull in vendored
PolygraphModular headers stay on that code's own warning style.

## What each test covers

Ten CTest cases exercise the `pm_framework` engine, and `ifs-clap` drives the built plugin through
the CLAP C API (`clap-validator`, below, covers CLAP conformance):

| CTest name       | Executable               | Verifies |
|------------------|--------------------------|----------|
| `pm`             | `pm-selftest`            | UMP MIDI layer: predicates, raw-word decode, microtonal pitch attribute, CLAP→UMP decoders |
| `pm-module`      | `pm-module-test`         | `Module` + `ModuleMap` core: param read/write, process proc, map add/get |
| `pm-sinosc`      | `pm-sinosc-test`         | The vendored sine oscillator produces an ~A440 sine |
| `pm-ifscore`     | `pm-ifscore-test`        | IFSCore: the per-point smoothed eval matches the rendered wavetable |
| `pm-adsr`        | `pm-adsr-test`           | ADSR envelope: note-on drives attack, note-off drives release to ~0 |
| `pm-modules`     | `pm-modules-test`        | Combine / Final / Slew compile+run; PitchConverter maps note→pitch |
| `pm-factory`     | `pm-factory-test`        | ModuleFactory + the trimmed init-specs build all six v1 modules |
| `pm-voice`       | `pm-voice-test`          | One full voice renders a note end-to-end and releases |
| `pm-modroute`    | `pm-modroute-test`       | Mod routing: connecting an LFO to IFSOsc's Bx input switches it to the eval path, which still renders |
| `pm-voicemanager`| `pm-voicemanager-test`   | Polyphony: a 3-note chord allocates 3 voices and frees them on release; CC 123 releases only its channel's notes, pedal-held ones too; CC 120 silences its channel at once, with no tail, while another channel plays on, and the next note sounds |
| `ifs-clap`       | `ifs-clap-host-test`     | The built `.clap`: ports, note dialects (MIDI 2.0 preferred), params, state (incl. the legacy DPF blob and loading while active), 7.9-pitch / CLAP / MIDI 1.0 notes, channel / per-note / RC 7 pitch bend (on both oscillators, past the glide, matched on note + channel), Pitch 7.25, CLAP tuning note expressions by key and by note id alone, all measured end to end; the MPE switch (member bend ±48, master bend added), RPN 0, and released notes (member bend frozen, master and MPE-off wheel still apply, key-addressed per-note ignored, CLAP id-addressed still applied); velocity (MIDI 1.0 127 = CLAP 1.0; each Velocity setting's level at velocity 64); a MIDI 2.0 velocity-0 note-on playing; NOTE_END after the release (CLAP notes only, echoing id/key/channel/port), output events in time order, and voice-info; Pressure and Timbre picked in a source dropdown (channel pressure, MIDI 1.0 poly aftertouch and CLAP Pressure; CC74 per channel and CLAP Brightness overriding it; another channel's values ignored; under MPE the master channel's reaching a member note) |

## Running a single test

```sh
ctest --test-dir build -R pm-voice --output-on-failure   # by name (regex)
./build/pm-voice-test                                     # or run the binary directly
```

Each test binary prints diagnostic values (e.g. `Voice: peak=0.87 tail=0.0000`) and ends with
`ALL PASS`; CTest keys success on that string.

## CLAP conformance

Validate the built plugin against the CLAP spec with
[`clap-validator`](https://github.com/free-audio/clap-validator):

```sh
clap-validator validate build/IFSynth.clap
```

## Continuous integration

`.github/workflows/ci.yml` runs the configure + build + `ctest` on every push across
Linux (gcc/clang) and macOS (clang) with `-DIFSYNTH_WERROR=ON`.
