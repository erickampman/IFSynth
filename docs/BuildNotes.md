cmake --build build --target install-clap

cmake --build build -j

CLEAN 

cmake --build build -j --clean-first


Config Dump build:

    cd ~/dev-clap/IFSynth
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DIFSYNTH_PRESET_DUMP=ON
cmake --build build -j"$(nproc)"