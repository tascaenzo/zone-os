[BITS 64]

[EXTERN arch_interrupts_dispatch]
[GLOBAL isr_stub_table]
[GLOBAL _isr_common_stub]

section .text

; ----------------------------------------
; Macros per salvare e ripristinare i registri
; ----------------------------------------

%macro pusha64 0
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro popa64 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

; ----------------------------------------
; Tabella globale degli stub ISR
; ----------------------------------------

isr_stub_table:
%assign i 0
%rep 48
    dq isr_stub_%+i
%assign i i+1
%endrep

; ----------------------------------------
; Macros ISR per vettori con/senza error code
; ----------------------------------------

%macro ISR_NOERR 1
[GLOBAL isr_stub_%1]
isr_stub_%1:
    push 0                  ; Inserisce un finto error code
    push %1                 ; Inserisce il numero del vettore
    jmp _isr_common_stub
%endmacro

%macro ISR_ERR 1
[GLOBAL isr_stub_%1]
isr_stub_%1:
    push %1                 ; Inserisce il numero del vettore
    jmp _isr_common_stub
%endmacro

; ----------------------------------------
; ISR 0–31 (CPU Exceptions)
; ----------------------------------------

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; ----------------------------------------
; ISR 32–47 (PIC IRQs, timer, keyboard, etc.)
; ----------------------------------------

%assign i 32
%rep 16
    ISR_NOERR i
%assign i i+1
%endrep

; ----------------------------------------
; Common interrupt handler dispatcher
; ----------------------------------------

_isr_common_stub:
    cld                         ; Pulisce il direction flag
    pusha64                     ; Salva tutti i registri generali

    mov rsi, rsp                ; rsi = ctx (puntatore alla struttura salvata)
    mov rdi, [rsp + 120]        ; rdi = vector number (sopra l'error code)

    call arch_interrupts_dispatch
    mov rsp, rax                ; Se arch_dispatch restituisce uno stack modificato

    popa64
    add rsp, 16                 ; Ripristina stack (pop vector + error code)

    iretq                       ; Ritorna dall'interrupt
