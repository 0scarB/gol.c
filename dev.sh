#!/bin/sh

set -eu

COMPILE=1
EXEC=2
DEV=$(( COMPILE | EXEC ))

compile() {
    #
    # x86-64 Linux
    #
    out_file='gol-x86-64-bit-linux'
    gcc -ggdb -march=x86-64 -DLINUX_64_BIT \
        -Werror \
        -static \
        -nostartfiles -nostdlib \
        -s \
        -fno-unwind-tables -fno-asynchronous-unwind-tables \
        -fno-ident \
        -fno-ident \
        -Wl,-z,norelro \
        -Wl,--hash-style=sysv \
        -Wl,--build-id=none \
        gol.c \
        -o "$out_file"
    strip --remove-section=.note.gnu.property "$out_file"
    echo "Compiled ${out_file} for x86, 64-bit, Linux. Size in bytes: $( \
        wc -c "$out_file" | cut -d ' ' -f1 )"

    #
    # x86 32-bit Linux
    #
    out_file='gol-x86-32-bit-linux'
    gcc -m32 -march=i686 -DLINUX_32_BIT \
        -ggdb \
        -Werror \
        -static \
        -nostartfiles -nostdlib \
        -Os \
        -s \
        -fno-unwind-tables -fno-asynchronous-unwind-tables \
        -fno-ident \
        -Wl,-z,norelro \
        -Wl,--hash-style=sysv \
        -Wl,--build-id=none \
        gol.c \
        -o "$out_file"
    strip --remove-section=.note.gnu.property --remove-section=.got.plt "$out_file"
    echo "Compiled ${out_file} for x86, 32-bit, Linux. Size in bytes: $( \
        wc -c "$out_file" | cut -d ' ' -f1 )"

    #
    # Web = WebAssembly + HTML
    #
    out_file='gol-browser.html'

    # Compile C to WebAssembly binary
    clang --target=wasm32 -DBROWSER \
        -Werror \
        -nostartfiles -nostdlib \
        -Wl,--no-entry \
        -Os \
        -static \
        -s \
        gol.c \
        -o gol-browser.wasm

    # Embed WebAssembly as base64-encoded string in the HTML file
    embed_line_no="$(                               \
        grep --line-number WASM_BASE64 gol-browser.html \
        | cut -d ':' -f1                            \
        | tail --lines 1                            )"
    head -n "$(( $embed_line_no - 1 ))" gol-browser.html              > gol-browser-new.html
    echo "var WASM_BASE64=\"$( base64 --wrap 0 gol-browser.wasm )\"" >> gol-browser-new.html
    tail   +"$(( $embed_line_no + 1 ))" gol-browser.html             >> gol-browser-new.html
    mv gol-browser-new.html "$out_file"

    # Remove WebAssembly binary
    rm gol-browser.wasm

    echo "Compiled ${out_file} for the Browser. Size in bytes: $( \
        wc -c "$out_file" | cut -d ' ' -f1 )"

    # Copy HTML to github pages root
    cp gol-browser.html "docs/index.html"
}

exec_() {
    ./gol-x86-64-bit-linux
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

