#include "print.h"
#include "limine.h"
#include <stdint.h>

void kernel_main(void)
{

  init_print();

  while (1)
  {
    __asm__("hlt");
  }
}
