/* Linker script for the STM32F767ZI (2 MB flash, 512 KB SRAM).
 *
 * Written rather than recovered. The Ac6/System Workbench script that used to
 * live here carried a no-redistribution notice, sized RAM at 320 KB (those are
 * F746 numbers -- the F767ZI has 512 KB), and discarded every section of
 * libc.a, libm.a and libgcc.a.
 *
 * RAM is one region because the F767ZI's three blocks are address-contiguous:
 * DTCM 128 KB @0x20000000, SRAM1 368 KB @0x20020000, SRAM2 16 KB @0x2007C000.
 * Split them if DMA ever needs to avoid DTCM, which is reachable by DMA only
 * through the CPU's AHBS slave port (RM0410).
 */

ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 512K
}

/* The reset vector loads SP from the first word of the vector table. */
_estack = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size = 0x200;   /* newlib's malloc arena, reached via _sbrk */
_Min_Stack_Size = 0x400;

SECTIONS
{
  /* The boot ROM reads the vector table from the start of flash. Nothing
     references it, so without KEEP --gc-sections removes it and the chip
     boots into whatever follows. */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)

    KEEP(*(.init))
    KEEP(*(.fini))

    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  /* Unwind tables. -fno-exceptions means these should be empty; keeping the
     sections costs nothing and avoids a link error if a library brings some. */
  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  /* Static constructors. __libc_init_array walks these; KEEP is what makes
     namespace-scope objects with non-trivial constructors actually run. */
  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >FLASH

  /* Initialized data: stored in flash, copied to RAM by Reset_Handler.
     _sidata is the load address it copies from. */
  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  . = ALIGN(4);
  .bss :
  {
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* _sbrk starts the heap at `end` and refuses to grow past __heap_limit,
     so a runaway allocation returns ENOMEM instead of quietly overwriting
     the stack growing down from _estack. */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  __heap_limit = _estack - _Min_Stack_Size;

  /DISCARD/ : { libc.a(*.note.GNU-stack) }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}

ASSERT(_estack - _ebss >= _Min_Heap_Size + _Min_Stack_Size,
       "RAM exhausted: not enough room left for the heap and stack")
