; boot/boot.asm - Entry point kernel 64-bit con Limine già in long mode

section .text
    global _start
    extern kmain

_start:
    ; Setup dello stack
    mov rsp, stack_top
    and rsp, -16        ; Stack alignment a 16 byte (SysV ABI)

    call kmain    ; Entra nel C kernel

    cli
.halt:
    hlt
    jmp .halt

section .bss
    align 16
stack_bottom:
    resb 16384          ; 16 KiB stack
stack_top:

section .note.GNU-stack noalloc noexec nowrite
