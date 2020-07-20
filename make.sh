# !/bin/sh

#if there's no build folder, create one
[ ! -d "./build" ] && mkdir build

cd ./build

#clean the old building file
rm -rf *
rm ../src/inc/people_counter.h

if [[ "$OSTYPE" == "darwin"* ]]; then
	cmake -C ../header_config.txt -DCMAKE_CXX_FLAGS="-Wno-error=narrowing" ..
else
	cmake -C ../header_config.txt ..
fi

make -j12



