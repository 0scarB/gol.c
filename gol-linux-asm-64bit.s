.global syscall

.text
syscall:
    ; See: https://wiki.osdev.org/System_V_ABI#x86-64
    mov %rdi, %rax
    mov %rsi, %rdi
    mov %rdx, %rsi
    mov %rcx, %rdx
    syscall
    ret

