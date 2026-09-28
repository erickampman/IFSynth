CONFIGURE

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

BUILD (also symlinks build/bin/IFSynth.clap into ~/.clap via a post-build step)

cmake --build build -j

CLEAN 

cmake --build build -j --clean-first


Config Dump build:

    cd ~/dev/dev-clap/IFSynth
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DIFSYNTH_PRESET_DUMP=ON
cmake --build build -j"$(nproc)"
