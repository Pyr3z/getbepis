#! /bin/bash
# lol was gonna use a makefile, but let's just keep it simple

SRC=("src/getbepis.c")
OUT="bin/3pw"

CC=${1:-$(which gcc)}
shift

if [[ $# -eq 0 ]]; then # default gcc [and clang] args
  CCARGS=("-ansi" "-Wall" "-Wextra" "-pedantic")
  CCARGS+=("-Wno-implicit-fallthrough")
  CCARGS+=("-Werror")
else
  CCARGS=$@
fi

mkdir -p ${OUT%/*}

# compile
"$CC" ${CCARGS[@]} ${SRC[@]} -o "$OUT"

# if we have a defined /c/path directory, we probably on wsl => copy exe there
# if [[ $? -eq 0 && -d '/mnt/c/path' ]]; then
#   cp "$OUT" '/mnt/c/path'
# fi
