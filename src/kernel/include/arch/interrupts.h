#pragma once
#include <lib/types.h>

typedef struct __attribute__((packed)) {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rsi, rdi, rbp, rdx, rcx, rbx, rax;
    u64 int_no, err_code;
    u64 rip, cs, rflags, rsp, ss;
} isr_frame_t;

void idt_init(void);
void pic_remap(u8 offset1, u8 offset2);
void pic_send_eoi(u8 irq);
void isr_common_handler(isr_frame_t *frame);
