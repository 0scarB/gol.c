.global syscall

.text
syscall:
    ; See: https://wiki.osdev.org/System_V_ABI#i386
    mov 4(%esp), %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    mov 16(%esp), %edx
    int $0x80
    ret
