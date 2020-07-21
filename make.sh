# !/bin/sh

echo "Please enter build mode: 1 for Debug, 2 for Release"

read option

mode=""

if [ $option == 1 ] 
then
	mode="-DCMAKE_BUILD_TYPE=Debug "
fi

echo "Compilation mode: $mode"

#if there's no build folder, create one
[ ! -d "./build" ] && mkdir build

cd ./build

#clean the old building file
rm -rf *
rm ../src/inc/people_counter.h

if [[ "$OSTYPE" == "darwin"* ]]; then
	cmake -C ../header_config.txt -DCMAKE_CXX_FLAGS="-Wno-error=narrowing" $mode..
else
	cmake -C ../header_config.txt $mode..
fi

make -j12



