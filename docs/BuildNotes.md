cmake --build build --target install-clap

cmake --build build -j

CLEAN 

cmake --build build -j --clean-first