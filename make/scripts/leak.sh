#!/bin/bash

# get in the right folder
path="$(pwd)/$0"
folder=$(dirname "$path")
cd "$folder"/../../build || exit

if [ -z "$1" ]; then
	echo "usage: $0 ./build/program"
	exit
fi

valgrind+=("--show-error-list=yes")
valgrind+=("--show-leak-kinds=all")
valgrind+=("--track-origins=yes")
valgrind+=("--leak-check=full")
valgrind+=("--suppressions=../res/valgrind/valgrind.supp")

export DEBUGINFOD_URLS="https://debuginfod.archlinux.org"
valgrind "${valgrind[@]}" 2> ../valgrind.log ../"$1"
