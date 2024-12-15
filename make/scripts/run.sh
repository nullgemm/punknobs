#!/bin/bash

# get in the right folder
path="$(pwd)/$0"
folder=$(dirname "$path")
cd "$folder"/../.. || exit

# get params
build_type=$1
build_backend=$2
build_example=$3
build_toolchain=$4

# set params default values if needed
if [ -z "$build_type" ]; then
	build_type=development
fi

if [ -z "$build_backend" ]; then
	build_backend=macos
fi

if [ -z "$build_example" ]; then
	build_example=macos
fi

if [ -z "$build_toolchain" ]; then
	build_toolchain=native
fi

# compile
echo -e "> compiling" \
"(mode: \"$build_type\"" \
"backend: \"$build_backend\"" \
"example: \"$build_example\"" \
"toolchain: \"$build_toolchain\")" \

./make/scripts/build.sh "$build_type" "$build_backend" "$build_example" "$build_toolchain" || exit

# run
if [ "$build_example" = "macos" ]; then
echo -e "> running \"punknobs_example_""$build_example""_""$build_toolchain\""
./build/punknobs_example_"$build_example"_"$build_toolchain"
else
echo -e "> running \"punknobs_example_$build_example\""
./build/punknobs_example_"$build_example"
fi
echo -ne "\n"
