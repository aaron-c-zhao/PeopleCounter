# !/bin/sh

#if there's no build folder, create one
[ ! -d "./build" ] && mkdir build

cd ./build

rm -rf *
if [[ "$OSTYPE" == "darwin"* ]]; then
	cmake -DCMAKE_CXX_FLAGS="-Wno-error=narrowing" ..
else
	cmake ..
fi

make -j12



