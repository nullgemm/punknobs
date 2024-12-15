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

# generate ninja files
case $build_backend in
	evdev_poll)
		rm -rf build make/output
		./make/lib/elf.sh "$build_type"
		./make/lib/evdev.sh "$build_type" poll

		if [ "$build_example" != "none" ]; then
			./make/example/evdev.sh "$build_type" poll
		fi
	;;

	evdev_epoll)
		rm -rf build make/output
		./make/lib/elf.sh "$build_type"
		./make/lib/evdev.sh "$build_type" epoll

		if [ "$build_example" != "none" ]; then
			./make/example/evdev.sh "$build_type" epoll
		fi
	;;

	macos)
		rm -rf build make/output
		./make/lib/macho.sh "$build_type" "$build_toolchain"
		./make/lib/macos.sh "$build_type" "$build_toolchain"

		if [ "$build_example" != "none" ]; then
			./make/example/macos.sh "$build_type" "$build_toolchain"
		fi
	;;

	win)
		rm -rf build make/output
		./make/lib/pe.sh "$build_type" "$build_toolchain"
		./make/lib/win.sh "$build_type" "$build_toolchain"

		if [ "$build_example" != "none" ]; then
			./make/example/win.sh "$build_type" "$build_toolchain"
		fi
	;;

	*)
		echo "invalid backend: $build_backend"
		exit 1
	;;
esac

# build
case $build_backend in
	evdev_poll | evdev_epoll)
		samu -f ./make/output/lib_elf.ninja
		samu -f ./make/output/lib_evdev.ninja

		samu -f ./make/output/lib_elf.ninja headers
		samu -f ./make/output/lib_evdev.ninja headers

		if [ "$build_example" != "none" ]; then
			samu -f ./make/output/example_evdev.ninja
		fi
	;;

	macos)
		samu -f ./make/output/lib_macho.ninja
		samu -f ./make/output/lib_macos.ninja

		samu -f ./make/output/lib_macho.ninja headers
		samu -f ./make/output/lib_macos.ninja headers

		if [ "$build_example" != "none" ]; then
			samu -f ./make/output/example_macos.ninja
		fi
	;;

	win)
		ninja -f ./make/output/lib_pe.ninja
		ninja -f ./make/output/lib_win.ninja

		ninja -f ./make/output/lib_pe.ninja headers
		ninja -f ./make/output/lib_win.ninja headers

		if [ "$build_example" != "none" ]; then
			ninja -f ./make/output/example_win.ninja
		fi
	;;

	*)
		echo "invalid backend: $build_backend"
		exit 1
	;;
esac
