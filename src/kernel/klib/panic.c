// #include <arch/cpu.h>
// #include <arch/interrupt_context.h>
// #include <drivers/video/console.h>
// #include <interrupts/interrupts.h>
// #include <klib/klog.h>
// #include <lib/stdarg.h>
// #include <lib/stdio.h>
//
// #define PANIC_FG_COLOR CONSOLE_COLOR_WHITE
// #define PANIC_BG_COLOR CONSOLE_COLOR_BLACK
//
// // Stampa una sezione visiva elegante
// static void draw_banner(void) {
//   console_write("==================================================\n");
//   console_write("                  !! KERNEL PANIC !!              \n");
//   console_write("==================================================\n\n");
// }
//
// // Stampa una descrizione testuale del tipo di eccezione, se nota
// static void print_verbose_exception_message(u8 vector) {
//   const char *name = interrupts_exception_name(vector);
//
//   console_write(">>> DESCRIZIONE DELL'ECCEZIONE\n");
//
//   if (!name)
//     name = "Sconosciuta";
//
//   char msg[128];
//   ksnprintf(msg, sizeof(msg), " Tipo: %s (vector #%u)\n", name, vector);
//   console_write(msg);
//
//   switch (vector) {
//   case 0:
//     console_write(" Division by Zero: Tentativo di dividere per zero.\n");
//     break;
//   case 6:
//     console_write(" Invalid Opcode: Istruzione illegale o sconosciuta.\n");
//     break;
//   case 13:
//     console_write(" General Protection Fault: Violazione di protezione.\n");
//     break;
//   case 14:
//     console_write(" Page Fault: Accesso a pagina non mappata o protetta.\n");
//     break;
//   case 3:
//     console_write(" Breakpoint: Punto di interruzione raggiunto.\n");
//     break;
//   case 1:
//     console_write(" Debug Exception: Trigger di debug attivato.\n");
//     break;
//   case 8:
//     console_write(" Double Fault: Errore durante la gestione di un altro errore.\n");
//     break;
//   default:
//     console_write(" Nessuna descrizione disponibile per questa eccezione.\n");
//     break;
//   }
//
//   console_write("\n");
// }
//
// // Panic semplice, senza contesto CPU
// __attribute__((noreturn)) void panic(const char *fmt, ...) {
//   console_set_color(PANIC_FG_COLOR, PANIC_BG_COLOR);
//   console_clear();
//
//   draw_banner();
//
//   char msg[512];
//   va_list args;
//   va_start(args, fmt);
//   ksnprintf(msg, sizeof(msg), fmt, args);
//   va_end(args);
//
//   console_write(msg);
//   console_write("\n\n");
//   console_write("Il sistema è stato bloccato. Premi RESET per riavviare.\n");
//
//   while (1)
//     __asm__ volatile("cli; hlt");
// }
//
// // Panic completo con contesto CPU
// __attribute__((noreturn)) void panic_with_ctx(const char *fmt, const arch_interrupt_context_t *ctx, ...) {
//   console_set_color(PANIC_FG_COLOR, PANIC_BG_COLOR);
//   console_clear();
//
//   draw_banner();
//
//   char msg[512];
//   va_list args;
//   va_start(args, ctx);
//   ksnprintf(msg, sizeof(msg), fmt, args);
//   va_end(args);
//
//   console_write(msg);
//   console_write("\n\n");
//
//   if (ctx) {
//     print_verbose_exception_message((u8)ctx->interrupt_vector);
//
//     char buf[128];
//
//     console_write(">>> CPU CONTEXT\n");
//     ksnprintf(buf, sizeof(buf), " RIP = 0x%016lx   RSP = 0x%016lx   RFLAGS = 0x%016lx\n", ctx->rip, ctx->rsp, ctx->rflags);
//     console_write(buf);
//     ksnprintf(buf, sizeof(buf), " CS  = 0x%04lx      SS = 0x%04lx\n", ctx->cs, ctx->ss);
//     console_write(buf);
//     ksnprintf(buf, sizeof(buf), " VECTOR = %lu    ERR = 0x%016lx\n", ctx->interrupt_vector, ctx->error_code);
//     console_write(buf);
//
//     console_write("\n>>> REGISTRI\n");
//     ksnprintf(buf, sizeof(buf), " RAX = 0x%016lx  RBX = 0x%016lx  RCX = 0x%016lx  RDX = 0x%016lx\n", ctx->rax, ctx->rbx, ctx->rcx, ctx->rdx);
//     console_write(buf);
//     ksnprintf(buf, sizeof(buf), " RSI = 0x%016lx  RDI = 0x%016lx  RBP = 0x%016lx\n", ctx->rsi, ctx->rdi, ctx->rbp);
//     console_write(buf);
//     ksnprintf(buf, sizeof(buf), " R8  = 0x%016lx  R9  = 0x%016lx  R10 = 0x%016lx  R11 = 0x%016lx\n", ctx->r8, ctx->r9, ctx->r10, ctx->r11);
//     console_write(buf);
//     ksnprintf(buf, sizeof(buf), " R12 = 0x%016lx  R13 = 0x%016lx  R14 = 0x%016lx  R15 = 0x%016lx\n", ctx->r12, ctx->r13, ctx->r14, ctx->r15);
//     console_write(buf);
//   }
//
//   console_write("\n==================================================\n");
//   console_write(" Il sistema è stato arrestato per protezione.\n");
//   console_write(" Premi RESET per riavviare.\n");
//   console_write("==================================================\n");
//
//   while (1)
//     __asm__ volatile("cli; hlt");
// }
//