ENTRY(_start)

SECTIONS {
  . = 0xFFFFFFFF80000000;

  .text : ALIGN(4K) {
    *(.text*)
  }

  .rodata : ALIGN(4K) {
    *(.rodata*)
  }

  .data : ALIGN(4K) {
    *(.data*)
  }

  .bss : ALIGN(4K) {
    *(.bss*)
    *(COMMON)
  }
}
