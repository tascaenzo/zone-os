BITS 64
section .text

global isr_stub_table
extern isr_common_stub
extern isr_common_handler

isr_stub_table:
%assign i 0
%rep 32
    dq isr%+i
%assign i i+1
%endrep
%assign j 0
%rep 16
    dq irq%+j
%assign j j+1
%endrep

%macro ISR_NOERR 1
isr%1:
    push 0
    push %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERR 1
isr%1:
    push %1
    jmp isr_common_stub
%endmacro

%macro IRQ 2
irq%2:
    push 0
    push %1
    jmp isr_common_stub
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR 8
ISR_NOERR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR 21
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

IRQ 32,0
IRQ 33,1
IRQ 34,2
IRQ 35,3
IRQ 36,4
IRQ 37,5
IRQ 38,6
IRQ 39,7
IRQ 40,8
IRQ 41,9
IRQ 42,10
IRQ 43,11
IRQ 44,12
IRQ 45,13
IRQ 46,14
IRQ 47,15

isr_common_stub:
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
    mov rdi, rsp
    call isr_common_handler
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
    add rsp, 16
    iretq

section .note.GNU-stack noalloc noexec nowrite
