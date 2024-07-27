#!/bin/sh

set -eu

COMPILE=1
EXEC=2
DEV=$(( COMPILE | EXEC ))

compile() {
    gcc -march=x86-64 -DLINUX_64_BIT \
        -Werror \
        -Os \
        -static \
        -s \
        -nostartfiles -nostdlib \
        -fno-unwind-tables -fno-asynchronous-unwind-tables \
        -fno-ident \
        -Wl,-z,norelro \
        -Wl,--hash-style=sysv \
        -Wl,--build-id=none \
        gol.c \
        -o gol-x86-64-linux
    strip --remove-section=.note.gnu.property gol-x86-64-linux
    echo "Compiled x86-64 linux. Size in bytes: $( wc -c gol-x86-64-linux )"

    gcc -m32 -march=i686 -DLINUX_32_BIT \
        -Werror \
        -Os \
        -static \
        -s \
        -nostartfiles -nostdlib \
        -fno-unwind-tables -fno-asynchronous-unwind-tables \
        -fno-ident \
        -Wl,-z,norelro \
        -Wl,--hash-style=sysv \
        -Wl,--build-id=none \
        gol.c \
        -o gol-i686-linux
    strip --remove-section=.note.gnu.property --remove-section=.got.plt gol-i686-linux
    echo "Compiled x86 32-bit linux. Size in bytes: $( wc -c gol-i686-linux )"
}

exec_() {
    ./gol-x86-64-linux
}

main() {
    flags="${1:-${DEV}}"
    for arg in "$@"; do
        if [ $arg = 'compile' ]; then
            flags=$(( $flags | COMPILE ))
        fi
        if [ $arg = 'exec' ]; then
            flags=$(( $flags | EXEC ))
        fi
        if [ $arg = 'dev' ]; then
            flags=$(( $flags | DEV ))
        fi
    done

    if [ $(( $flags & $COMPILE )) -ne 0 ]; then
        compile
    fi
    if [ $(( $flags & $EXEC )) -ne 0 ]; then
        exec_
    fi
}

main $@

