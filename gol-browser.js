var SUPPORTS_TEXTDECODER = !!TextDecoder
// Only the main "loop" sets term.innerHTML. All other functions write/copy
// to `writeBuf` to increate performance
var writeBuf       = new Uint8Array(1024)
var writeBufOffset = 0
// We emulate line-buffered input. See `readInputChar` and `onkeydown` listener
var inputCharQueue = []
var bufferingLine  = true
var updateInterval = 0

var len = (x) => x.length

window.onload = async () => {
    // `term` stores the <pre id="terminal"> DOM element. term.innerHTML is use as
    // a fake terminal
    var term = document.getElementById("t")

    // Load WASM module from a base64 string
    var wasmExports = (await WebAssembly.instantiate(
        new Uint8Array([...atob(WASM_BASE64)].map(c => c.charCodeAt())),
        {env:{
            // Functions we call from C
            output(bufPtr, bufSz) {
                // Reallocate a bigger write buffer if needed
                while (writeBufOffset + bufSz > len(writeBuf)) {
                    var newWriteBuf = new Uint8Array(2*len(writeBuf))
                    newWriteBuf.set(writeBuf, 0, len(writeBuf))
                    writeBuf = newWriteBuf
                }
                // Copy as directly as we can from WASM memory to the write buffer
                writeBuf.set(new Uint8Array(wasmMem, bufPtr, bufSz), writeBufOffset)
                writeBufOffset += bufSz
            },
            clearTerm() {
                writeBufOffset = 0
                inputCharQueue = []
            },
            readInputChar() {
                if (len(inputCharQueue) > 0) {
                    // Buffer until newline;
                    if (bufferingLine) return 0
                    // then return buffered characters
                    var c = inputCharQueue.shift()
                    return c
                }
                // Reset
                bufferingLine = true
                return 0
            },
            setUpdateInterval(secs) {
                updateInterval = secs
            },
            random() {
                return Math.random()
            },
        }
    })).instance.exports
    var wasmMem = wasmExports.memory.buffer

    // Call the `update` C function in a main-"loop" and set term.innerHTML from
    // the contents of `writeBuf`
    var uf8decoder = new TextDecoder()
    var t0 = 0;
    (async function loop(t1) {
        if (t1 - t0 > updateInterval*1000) {
            t0 = t1
            wasmExports.update()
            term.innerHTML = SUPPORTS_TEXTDECODER
                ? uf8decoder.decode(writeBuf.subarray(0, writeBufOffset))
                : [...subBuf].map(b => String.fromCharCode(b)).join("")
        }
        requestAnimationFrame(loop);
    })()
}

window.onkeydown = ({keyCode, key}) => {
    switch (keyCode) {
        case 8:  // Backspace
            if (len(inputCharQueue) > 0) {
                writeBufOffset--
                inputCharQueue.pop()
            }
            return
        case 13: // Enter
            key = '\n'
            bufferingLine = false
        default:
            if (len(key) > 1) return
    }
    var charCode = key.charCodeAt()
    writeBuf[writeBufOffset++] = charCode
    inputCharQueue.push(charCode);
}

var WASM_BASE64="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"
