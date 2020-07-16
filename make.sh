# !/bin/sh

#if there's no build folder, create one
[ -d "./build" ] && mkdir build

cd ./build

rm -rf *

cmake ..

make -j12



