ENTRY(_start)

KERNEL_BASE = 0xFFFFFFFF80000000;

SECTIONS {
  . = KERNEL_BASE;

  _kernel_start = .;

  .text : ALIGN(4K) {
    _text_start = .;
    *(.text*)
    _text_end = .;
  }

  .rodata : ALIGN(4K) {
    _rodata_start = .;
    *(.rodata*)
    _rodata_end = .;
  }

  .data : ALIGN(4K) {
    _data_start = .;
    *(.data*)
    _data_end = .;
  }

  .bss : ALIGN(4K) {
    _bss_start = .;
    *(.bss*)
    *(COMMON)
    _bss_end = .;
  }

  _kernel_end = .;

  /DISCARD/ : {
    *(.comment)
    *(.note*)
  }
}
