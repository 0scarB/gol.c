var SUPPORTS_TEXTDECODER = !!TextDecoder
// Only the main "loop" sets term.innerHTML. All other functions write/copy
// to `writeBuf` to increate performance
var writeBuf       = new Uint8Array(1024)
var writeBufOffset = 0
// We emulate line-buffered input. See `readInputChar` and `onkeydown` listener
var inputCharQueue = []
var bufferingLine  = true

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
                    return inputCharQueue.shift()
                }
                // Reset
                bufferingLine = true
                return 0
            }
        }
    })).instance.exports
    var wasmMem = wasmExports.memory.buffer

    // Call the `update` C function in a main-"loop" and set term.innerHTML from
    // the contents of `writeBuf`
    var uf8decoder = new TextDecoder()
    var t0 = 0;
    (async function loop(t1) {
        if (t1 - t0 > 0.1*1000) {
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

var WASM_BASE64="AGFzbQEAAAABGgVgAABgAn9/AX9gAn9/AGAAAX9gBH9/f38AAjIDA2VudgZvdXRwdXQAAgNlbnYJY2xlYXJUZXJtAAADZW52DXJlYWRJbnB1dENoYXIAAwMFBAABBAEFAwEAAgcTAgZtZW1vcnkCAAZ1cGRhdGUAAwqfCASUBQERfwJAQbAJLQAARQRAQY4IQcCJAxAEDQFBgAhBwokDEAQNAUHEiQNBwBE2AgBBwokDLwEAQcCJAy8BAGxBAnZBhAhqQf//A3EiAEGA+AJPBEBBzghBIhAAQcKJA0EAOwEAQcCJA0EAOwEADwtByIkDIABBwAlqNgIAQcyJA0GAgAMgAGtBgHhxNgIAQZgJIQMDQCABQQNHBEBBACEAA0AgAEEDRgRAIANBA2ohAyABQQFqIQEMAwVBACAAQf//A3EgAUH//wNxIAAgA2otAAAQBSAAQQFqIQAMAQsACwALC0GwCUEBOgAACxABQciJAygCACECA0AgBcEiAEHCiQMvAQBIBEAgAEEBaiEKIAVBAWshC0EAIQQDQCAEwSIAQcCJAy8BACIITgRAIAJBCjoAACAFQQFqIQUgAkEBaiECDAMFIABBAWohDCAEQQFrIQZBACEDIARB//8DcSINIAVB//8DcSIOEAYhB0HCiQMvAQAhCSALIQEDQAJAIAHBIgAgCkoEQEEBIA0gDiADIAdrQf8BcSIAQQNHBH8gAEECRiAHQQBHcQVBAQsQBUGhCUGlCSAHGyEBIAIhAANAIAEtAAAiBkUNAiAAIAY6AAAgAkEBaiECIABBAWohACABQQFqIQEMAAsACyAAIAlqIQ8gBiEAA0AgAMEiECAMSgRAIAFBAWohAQwDBSAIIBBqIAhvQf//A3EgDyAJb0H//wNxEAYgA2ohAyAAQQFqIQAMAQsACwALCyAEQQFqIQQgAkHIiQMoAgAiAmsiAUHMiQMoAgBLBH8gAiABEABByIkDKAIABSAACyECDAELAAsACwtByIkDKAIAIgAgAiAAaxAAQdCJA0HQiQMtAABBAXM6AAALC/sBAQJ/AkAgAS8BAA0AAkBB0okDLwEADQBB1IkDLQAAQQFxDQADQCACQf8BcSEDIAJBAWohAiAAIANqLQAADQALIAAgAkH/AXEQAEHUiQNBAToAAAtBASECEAIiAEUNACAAQf8BcUEKRgRAQdKJAy8BACIARQRAQfEIQSYQAEHUiQNBADoAAEEBDwsgASAAOwEAQdSJA0EAOgAAQdKJA0EAOwEAQQAPCyAAQTprQf8BcUH1AU0EQEGcCEExEAADQBACQQpHDQALQdSJA0EAOgAAQdKJA0EAOwEAQQEPC0HSiQNB0okDLwEAQQpsIABBMGtB/wFxajsBAAsgAgtRAEHEiQMoAgBBwIkDLwEAIAJsIAFqIgFBAnZB//8DcWoiAiACLQAAIgJBASABQQF0QQZxQdCJAy0AACAAc3IiAHRyIAJBfiAAd3EgAxs6AAALOABBxIkDKAIAQcCJAy8BACABbCAAaiIAQQJ2Qf//A3FqLQAAQdCJAy0AACAAQQF0QQZxcnZBAXELC68BAQBBgAgLpwFHYW1lIGhlaWdodDogAEdhbWUgIHdpZHRoOiAASW52YWxpZCBwb3NpdGl2ZSB3aG9sZSBudW1iZXIuIFBsZWFzZSB0cnkgYWdhaW4hCgBHcmlkIHRvbyBsYXJnZS4gUGxlYXNlIHRyeSBhZ2FpbiEKAE51bWJlciBjYW5ub3QgYmUgMC4gUGxlYXNlIHRyeSBhZ2FpbiEKAAABAAAAAQEBASMjAAAuIAAsD3RhcmdldF9mZWF0dXJlcwIrD211dGFibGUtZ2xvYmFscysIc2lnbi1leHQ="
