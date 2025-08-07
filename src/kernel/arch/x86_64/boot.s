section .bss
    align 16
global kernel_stack_top
kernel_stack_bottom:
    resb 16384
kernel_stack_top:        ; 👈 anche usato come stack_top

section .text
    global _start
    extern kmain

_start:
    mov rsp, kernel_stack_top
    and rsp, -16            ; Allineamento stack per ABI

    call kmain

.halt:
    cli
    hlt
    jmp .halt
