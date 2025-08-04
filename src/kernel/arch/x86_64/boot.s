section .text
    global _start
    extern kmain

_start:
    ; Set up a temporary kernel stack
    mov rsp, stack_top
    and rsp, -16                ; Align stack to 16 bytes (SysV ABI requirement)

    ; Call the C entry point
    call kmain

    ; Halt if kmain returns (should never happen)
.halt:
    cli
    hlt
    jmp .halt

section .bss
    align 16
stack_bottom:
    resb 16384                  ; 16 KiB stack (adjust if needed)
stack_top:
