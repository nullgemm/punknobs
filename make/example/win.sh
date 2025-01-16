#!/bin/bash

# get into the script's folder
cd "$(dirname "$0")" || exit
cd ../..

# params
build=$1

function syntax {
echo "syntax reminder: $0 <build type>"
echo "build types: development, release, sanitized"
}

# utilitary variables
tag=$(git tag --sort v:refname | tail -n 1)
output="make/output"
name_lib="punknobs_win"

# ninja file variables
folder_ninja="build"
folder_objects="\$builddir/obj"
folder_punknobs="punknobs_bin_$tag"
folder_library="\$folder_punknobs/lib/punknobs"
folder_include="\$folder_punknobs/include"
name="punknobs_example_win"
cmd="./\$name"
cc="x86_64-w64-mingw32-gcc"
ld="x86_64-w64-mingw32-gcc"

# compiler flags
flags+=("-std=c99" "-pedantic")
flags+=("-Wall" "-Wextra" "-Werror=vla" "-Werror")
flags+=("-Wformat")
flags+=("-Wformat-security")
flags+=("-Wno-address-of-packed-member")
flags+=("-Wno-unused-parameter")
flags+=("-Wno-cast-function-type")
flags+=("-I\$folder_include")
flags+=("-Ires/egl")
flags+=("-Iexample/helpers")
defines+=("-DPUNKNOBS_EXAMPLE_WIN")
#defines+=("-DPUNKNOBS_EXAMPLE_LOG_ALL")

# customize depending on the chosen build type
if [ -z "$build" ]; then
	build=development
fi

case $build in
	development)
flags+=("-g")
	;;

	release)
flags+=("-D_FORTIFY_SOURCE=2")
flags+=("-fno-stack-protector")
flags+=("-fPIE")
flags+=("-fPIC")
flags+=("-O2")
	;;

	sanitized_memory)
flags+=("-g")
flags+=("-O1")
flags+=("-fno-omit-frame-pointer")
flags+=("-fno-optimize-sibling-calls")

flags+=("-fsanitize=leak")
flags+=("-fsanitize-recover=all")

ldflags+=("-fsanitize=leak")
ldflags+=("-fsanitize-recover=all")
	;;

	sanitized_undefined)
flags+=("-g")
flags+=("-O1")
flags+=("-fno-omit-frame-pointer")
flags+=("-fno-optimize-sibling-calls")

flags+=("-fsanitize=undefined")
flags+=("-fsanitize-recover=all")

ldflags+=("-fsanitize=undefined")
ldflags+=("-fsanitize-recover=all")
	;;

	sanitized_address)
flags+=("-g")
flags+=("-O1")
flags+=("-fno-omit-frame-pointer")
flags+=("-fno-optimize-sibling-calls")

flags+=("-fsanitize=address")
flags+=("-fsanitize-address-use-after-scope")
flags+=("-fsanitize-recover=all")

ldflags+=("-fsanitize=address")
ldflags+=("-fsanitize-address-use-after-scope")
ldflags+=("-fsanitize-recover=all")
	;;

	sanitized_thread)
flags+=("-g")
flags+=("-O1")
flags+=("-fno-omit-frame-pointer")
flags+=("-fno-optimize-sibling-calls")

flags+=("-fsanitize=thread")
flags+=("-fsanitize-recover=all")

ldflags+=("-fsanitize=thread")
ldflags+=("-fsanitize-recover=all")
	;;

	*)
echo "invalid build type"
syntax
exit 1
	;;
esac

ninja_file=example_win.ninja
src+=("example/example.c")
ldlibs+=("-ldinput8")
ldlibs+=("-lxinput")
ldlibs+=("-ldxguid")
# enable to prevent console from appearing
#ldlibs+=("-mwindows")

# additional object files
libs+=("\$folder_library/win/$name_lib.a")
libs+=("\$folder_library/punknobs_pe.a")

# default target
default+=("\$builddir/\$name")

# valgrind flags
valgrind+=("--show-error-list=yes")
valgrind+=("--show-leak-kinds=all")
valgrind+=("--track-origins=yes")
valgrind+=("--leak-check=full")
valgrind+=("--suppressions=../res/valgrind.supp")

# objcopy flags
objcopy+=("-I binary")
objcopy+=("-O elf64-x86-64")
objcopy+=("-B i386:x86-64")

# ninja start
mkdir -p "$output"

{ \
echo "# vars"; \
echo "builddir = $folder_ninja"; \
echo "folder_objects = $folder_objects"; \
echo "folder_punknobs = $folder_punknobs"; \
echo "folder_library = $folder_library"; \
echo "folder_include = $folder_include"; \
echo "name = $name"; \
echo "cmd = $cmd"; \
echo "cc = $cc"; \
echo "ld = $ld"; \
echo ""; \
} > "$output/$ninja_file"

# ninja flags
echo "# flags" >> "$output/$ninja_file"

echo -n "flags =" >> "$output/$ninja_file"
for flag in "${flags[@]}"; do
	echo -ne " \$\n$flag" >> "$output/$ninja_file"
done
echo -e "\n" >> "$output/$ninja_file"

echo -n "defines =" >> "$output/$ninja_file"
for define in "${defines[@]}"; do
	echo -ne " \$\n$define" >> "$output/$ninja_file"
done
echo -e "\n" >> "$output/$ninja_file"

echo -n "ldflags =" >> "$output/$ninja_file"
for flag in "${ldflags[@]}"; do
	echo -ne " \$\n$flag" >> "$output/$ninja_file"
done
echo -e "\n" >> "$output/$ninja_file"

echo -n "ldlibs =" >> "$output/$ninja_file"
for flag in "${ldlibs[@]}"; do
	echo -ne " \$\n$flag" >> "$output/$ninja_file"
done
echo -e "\n" >> "$output/$ninja_file"

echo -n "valgrind =" >> "$output/$ninja_file"
for flag in "${valgrind[@]}"; do
	echo -ne " \$\n$flag" >> "$output/$ninja_file"
done
echo -e "\n" >> "$output/$ninja_file"

echo -n "objcopy =" >> "$output/$ninja_file"
for flag in "${objcopy[@]}"; do
	echo -ne " \$\n$flag" >> "$output/$ninja_file"
done
echo -e "\n" >> "$output/$ninja_file"

# ninja rules
{ \
echo "# rules"; \
echo "rule cc"; \
echo "    deps = gcc"; \
echo "    depfile = \$out.d"; \
echo "    command = \$cc \$flags \$defines -MMD -MF \$out.d -c \$in -o \$out"; \
echo "    description = cc \$out"; \
echo ""; \
} >> "$output/$ninja_file"

{ \
echo "rule ld"; \
echo "    command = \$ld \$ldflags -o \$out \$in \$ldlibs"; \
echo "    description = ld \$out"; \
echo ""; \
} >> "$output/$ninja_file"

{ \
echo "rule leak"; \
echo "    command = cd \$builddir \$"; \
echo "    && valgrind \$valgrind 2> valgrind.log \$cmd \$"; \
echo "    && less valgrind.log"; \
echo "    description = running valgrind \$in"; \
echo ""; \
} >> "$output/$ninja_file"

{ \
echo "rule run"; \
echo "    command = cd \$builddir && \$cmd"; \
echo "    description = running \$in"; \
echo ""; \
} >> "$output/$ninja_file"

{ \
echo "rule clean"; \
echo "    command = make/scripts/clean.sh"; \
echo "    description = cleaning repo"; \
echo ""; \
} >> "$output/$ninja_file"

{ \
echo "rule generator"; \
echo "    command = make/example/win.sh $build"; \
echo "    description = re-generating the ninja build file"; \
echo ""; \
} >> "$output/$ninja_file"

# ninja targets
## compile sources
echo "# compile sources" >> "$output/$ninja_file"
for file in "${src[@]}"; do
	folder=$(dirname "$file")
	filename=$(basename "$file" .c)
	obj+=("\$folder_objects/$folder/$filename.o")
	{ \
	echo "build \$folder_objects/$folder/$filename.o: \$"; \
	echo "cc $file"; \
	echo ""; \
	} >> "$output/$ninja_file"
done

## archive objects
echo "# archive objects" >> "$output/$ninja_file"
echo -n "build \$builddir/\$name: ld" >> "$output/$ninja_file"
for file in "${obj[@]}" "${libs[@]}"; do
	echo -ne " \$\n$file" >> "$output/$ninja_file"
done
echo -e "\n" >> "$output/$ninja_file"

## special targets
{ \
echo "# run special targets"; \
echo "build leak: leak \$builddir/\$name"; \
echo "build run: run \$builddir/\$name"; \
echo "build regen: generator"; \
echo "build clean: clean"; \
echo "default" "${default[@]}"; \
} >> "$output/$ninja_file"
