#!/bin/sh

set -eu

COMPILE=1
EXEC=2
DEV=$(( COMPILE | EXEC ))

compile() {
    as --64 -gdwarf-5 gol-linux-asm-64bit.s -o gol-linux-asm-64bit.o
    as --32 -gdwarf-5 gol-linux-asm-32bit.s -o gol-linux-asm-32bit.o

    #
    # x86-64 Linux
    #
    out_file='gol-x86-64-bit-linux'
    gcc -march=x86-64 -DLINUX_64_BIT \
        -Werror --std=c99 -pedantic \
        -static \
        -nostartfiles -nostdlib \
        -Os \
        -s \
        -fno-unwind-tables -fno-asynchronous-unwind-tables \
        -fno-ident \
        -fno-stack-protector \
        -z execstack \
        -c gol.c \
        -o "$out_file.o"
    strip --strip-debug --remove-section=.note.gnu.property "$out_file.o"
    ld \
        --strip-all \
        --nmagic \
        --hash-style=sysv \
        --build-id=none \
        -z execstack \
        gol-linux-asm-64bit.o "$out_file.o" \
        -o "$out_file"

    echo "Compiled ${out_file} for x86, 64-bit, Linux. Size in bytes: $( \
        wc -c "$out_file" | cut -d ' ' -f1 )"

    #
    # x86 32-bit Linux
    #
    out_file='gol-x86-32-bit-linux'
    gcc -m32 -march=i386 -DLINUX_32_BIT \
        -Werror --std=c99 -pedantic \
        -static \
        -Os \
        -s \
        -fno-unwind-tables -fno-asynchronous-unwind-tables \
        -fno-ident \
        -fno-plt -fno-pic \
        -nostartfiles -nostdlib \
        -z execstack \
        -c gol.c \
        -o "$out_file.o"
    ld -m elf_i386 \
        --strip-all \
        --nmagic \
        --hash-style=sysv \
        --build-id=none \
        -z execstack \
        "$out_file.o" gol-linux-asm-32bit.o \
        -o "$out_file"
    echo "Compiled ${out_file} for x86, 32-bit, Linux. Size in bytes: $( \
        wc -c "$out_file" | cut -d ' ' -f1 )"
    rm "$out_file.o"

    # Remove object files
    rm *.o

    #
    # Web = WebAssembly + HTML
    #
    out_file='gol-browser.wasm'

    # Compile C to WebAssembly binary
    clang --target=wasm32 -DBROWSER \
        -Werror \
        -nostartfiles -nostdlib \
        -Wl,--no-entry \
        -Wl,--compress-relocations \
        -Oz \
        -static \
        -s \
        gol.c \
        -o gol-browser.wasm

    wasm-opt -Oz gol-browser.wasm -o gol-browser.wasm

    # Embed WebAssembly as base64-encoded string in the JavaScript file
    out_file='gol-browser.js'
    embed_line_no="$(                              \
        grep --line-number WASM_BASE64 "$out_file" \
        | cut -d ':' -f1                           \
        | tail --lines 1                           )"
    head -n "$(( $embed_line_no - 1 ))" "$out_file"                   > "$out_file.new"
    echo "var WASM_BASE64=\"$( base64 --wrap 0 gol-browser.wasm )\"" >> "$out_file.new"
    tail   +"$(( $embed_line_no + 1 ))" "$out_file"                  >> "$out_file.new"
    mv "$out_file.new" "$out_file"

    # Minify JavaScript
    terser \
        --compress passes=3,ecma=2015,hoist_vars=true,booleans_as_integers=true \
        --mangle toplevel=true \
        'gol-browser.js' \
        -o 'gol-browser.min.js'

    # Embed JavaScript in HTML
    out_file='gol-browser.html'
    embed_line_start="$(( $(                      \
        grep --line-number '<script>' "$out_file" \
        | cut -d ':' -f1                          \
        | tail --lines 1                          ) + 1 ))"
    embed_line_end="$(( $(                         \
        grep --line-number '</script>' "$out_file" \
        | cut -d ':' -f1                          \
        | tail --lines 1                          ) + 1 ))"
    head -n "$(( $embed_line_start - 1 ))" "$out_file"  > "$out_file.new"
    cat 'gol-browser.min.js'                           >> "$out_file.new"
    echo ''                                            >> "$out_file.new"
    tail   +"$(( $embed_line_end   - 1 ))" "$out_file" >> "$out_file.new"
    mv "$out_file.new" "$out_file"

    # Remove intermediate files
    rm gol-browser.wasm gol-browser.min.js

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

