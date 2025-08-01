#include <arch/exceptions.h>
#include <klib/klog.h>
#include <klib/panic.h>
#include <lib/stdio.h>

static const char *exception_messages[] = {
  "Division by zero", "Debug", "Non-maskable Interrupt", "Breakpoint",
  "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
  "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
  "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
  "x87 Floating-Point Exception", "Alignment Check", "Machine Check", "SIMD Exception",
  "Virtualization Exception", "Control Protection Exception", "Reserved", "Reserved",
  "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
  "Reserved", "Reserved"
};

void exceptions_handle(isr_frame_t *frame) {
  const char *name = "Unknown";
  if (frame->int_no < 32) {
    name = exception_messages[frame->int_no];
  }

  klog_info("Exception %lu: %s", frame->int_no, name);
  klog_info("RIP=0x%lx RSP=0x%lx RFLAGS=0x%lx", frame->rip, frame->rsp, frame->rflags);
  klog_info("Error Code=0x%lx", frame->err_code);

  bool critical = false;
  switch (frame->int_no) {
  case 6:  // Invalid Opcode
  case 8:  // Double Fault
  case 13: // General Protection Fault
  case 14: // Page Fault
    critical = true;
    break;
  default:
    break;
  }

  if (critical) {
    panic(name, frame);
  }
}

void trigger_div_zero(void) {
  volatile int zero = 0;
  volatile int one = 1;
  volatile int x = one / zero;
  (void)x;
}
