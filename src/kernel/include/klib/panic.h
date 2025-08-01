#pragma once
#include <arch/interrupts.h>

void panic(const char *message, isr_frame_t *frame) __attribute__((noreturn));
