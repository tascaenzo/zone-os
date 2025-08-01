#pragma once
#include <arch/interrupts.h>

void exceptions_handle(isr_frame_t *frame);

// Test helper
void trigger_div_zero(void);
