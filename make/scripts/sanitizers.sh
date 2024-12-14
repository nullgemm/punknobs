#!/bin/bash

# get in the right folder
path="$(pwd)/$0"
folder=$(dirname "$path")
cd "$folder"/../.. || exit

echo "> this script needs to sudo sysctl to work properly"
rnd_bits=$(sudo sysctl -n vm.mmap_rnd_bits)
rnd_threads=30

if [ "$rnd_bits" -ne "$rnd_threads" ]; then
	echo "> vm.mmap_rnd_bits is set to $rnd_bits, changing to $rnd_threads"
	sudo sysctl "vm.mmap_rnd_bits=$rnd_threads"
fi

./make/scripts/run.sh sanitized_memory
./make/scripts/run.sh sanitized_address
./make/scripts/run.sh sanitized_undefined
./make/scripts/run.sh sanitized_thread

if [ "$rnd_bits" -ne "$rnd_threads" ]; then
	echo "> re-setting vm.mmap_rnd_bits to $rnd_bits"
	sudo sysctl "vm.mmap_rnd_bits=$rnd_bits"
fi
