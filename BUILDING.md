# Building IFSynth

IFSynth builds the same way on Linux and macOS: CMake + Ninja, git submodules, C++20. The build
makes the plugin `IFSynth.clap`, the same plugin wrapped as `IFSynth.vst3` and, on macOS,
`IFSynth.component` (AUv2), and the tests.

## What you need

**macOS:** macOS 26 or later on Apple Silicon (arm64). Intel Macs aren't supported.

- The Xcode Command Line Tools: `xcode-select --install`. They bring clang, the macOS SDK, git and
  Python 3.
- [Homebrew](https://brew.sh), then `brew install cmake ninja`.
- Full Xcode isn't needed.

**Linux:** a C++20 compiler (GCC 11+ or Clang 14+), CMake 3.21+, Ninja, Python 3 and git. On Debian
or Ubuntu this installs them and every library the build needs:

```bash
sudo apt install build-essential cmake ninja-build pkg-config python3 git \
  libgl1-mesa-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxinerama-dev \
  libxcb1-dev libx11-xcb-dev libxcb-util-dev libxkbcommon-dev libxkbcommon-x11-dev \
  libfontconfig1-dev libasound2-dev libjack-jackd2-dev \
  libxfixes-dev libxi-dev libxss-dev libxtst-dev libwayland-dev wayland-protocols \
  libegl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev libdecor-0-dev libudev-dev \
  libdbus-1-dev libibus-1.0-dev
```

It's one list for every Unlikelyware project (the same one CI installs), so it covers more than
IFSynth needs. On other distros, install the same libraries under their own names.

## Get the source

```bash
git clone https://github.com/erickampman/IFSynth.git
cd IFSynth
tools/fetch_deps.sh
```

`tools/fetch_deps.sh` checks out the git submodules at their pinned commits, then applies the
clap-wrapper patch in `patches/`. Run it again after a `git pull` that moves, adds or re-points a
submodule: a plain pull leaves them where they were.

Use it rather than `git submodule update --init --recursive`: it fetches only the four VST3 SDK
submodules the build needs. The patch makes the AUv2 editor draw: clap-wrapper's AUv2 view never
calls `gui->show()`, which is where the editor starts its render timer.

## Build and test

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The same three commands on both platforms. Everything goes in `build/` (ignored by git).

The full test suite is described in [TESTING.md](TESTING.md).

This produces `build/IFSynth.clap` (a bundle on macOS), `build/IFSynth.vst3` (on Linux
`build/Release/IFSynth.vst3`) and, on macOS, `build/IFSynth.component` (AUv2 `aumu IFSy nLKL`). The
VST3 offers hosts the Tuning, Brightness and Pressure note expressions.

## Installing the plugin for hosts

A post-build step symlinks the CLAP into `~/.clap` (Linux) or `~/Library/Audio/Plug-Ins/CLAP`
(macOS), so a host picks up the freshest build after a rescan. The other formats are installed only
when you ask:

```bash
cmake --build build --target install-vst3
cmake --build build --target install-auv2     # macOS
```

`install-vst3` symlinks the VST3 into `~/.vst3` or `~/Library/Audio/Plug-Ins/VST3`; `install-auv2`
copies the component into `~/Library/Audio/Plug-Ins/Components` (AU hosts don't reliably follow
symlinks).

## Options

- `-DIFSYNTH_BUILD_VST3=OFF`, `-DIFSYNTH_BUILD_AUV2=OFF`: skip a format (AUv2 is on by default on
  macOS, and exists only there).
- `-DIFSYNTH_WERROR=ON`: warnings as errors.
- `-DIFSYNTH_PRESET_DUMP=ON`: a dev-only DUMP button that writes the current patch as JSON to
  `$HOME`.

## Checking a build

Validate the CLAP with [clap-validator](https://github.com/free-audio/clap-validator)
(`clap-validator validate build/IFSynth.clap`), and the AU with `auval -v aumu IFSy nLKL`.
