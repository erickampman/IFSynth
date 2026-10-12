CONFIGURE

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

BUILD (the .clap lands at build/IFSynth.clap; a post-build step symlinks it into ~/.clap)

cmake --build build -j

CLEAN 

cmake --build build -j --clean-first


Config Dump build:

    cd ~/dev/dev-clap/IFSynth
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DIFSYNTH_PRESET_DUMP=ON
cmake --build build -j"$(nproc)"
