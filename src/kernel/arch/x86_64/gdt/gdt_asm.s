; ==============================================================================
;  File: gdt_asm.s
;  Description: Routine per il caricamento della GDT e del TSS in x86_64 long mode
;               Usa NASM syntax. Questo codice è eseguito in early kernel init.
; ==============================================================================

global _load_gdt_and_tss_asm

section .text
align 16
_load_gdt_and_tss_asm:
    lgdt    [rdi]          ; carica il GDTR con la nuova GDT (rdi = GDT_Pointer*)

    mov     ax, 0x40       ; 0x40 = selettore del TSS (offset = 0x40, index 8)
    ltr     ax             ; carica il Task Register con il TSS

    mov     ax, 0x10       ; 0x10 = kernel data segment selector
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

    pop     rdi            ; recupera return address (da stack chiamante)
    mov     rax, 0x08      ; 0x08 = kernel code segment selector
    push    rax            ; push CS
    push    rdi            ; push RIP
    retfq                  ; far return → carica CS:RIP dallo stack
