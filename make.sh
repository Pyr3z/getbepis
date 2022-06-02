#! /bin/bash
# lol was gonna use a makefile, but let's just keep it simple

SRC=("src/getbepis.c")
OUT="bin/getbepis"

CC=${1:-$(which gcc)}
shift

if [[ $# -eq 0 ]]; then # default gcc [and clang] args
  # CCARGS=("-ansi" "-Wall" "-Wextra" "-pedantic")
  # CCARGS+=("-Werror")
  :
else
  CCARGS=$@
fi

mkdir -p ${OUT%/*}

# compile
"$CC" ${CCARGS[@]} ${SRC[@]} -o "$OUT"
