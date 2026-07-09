/*
 * DunitOS x86_64 userspace linker script.
 *
 * x86_64 userspace enters at main(kapi_t *, int, char **) directly; the
 * process wrapper and the desktop launcher already pass SysV arguments.
 */

ENTRY(main)

PHDRS
{
    text PT_LOAD FLAGS(5);  /* R-X */
    dynamic PT_DYNAMIC FLAGS(6);
}

SECTIONS
{
    . = 0x0;

    .text : {
        *(.text.start)
        *(.text*)
    } :text

    .rodata : {
        *(.rodata*)
    } :text

    .data : {
        *(.data*)
    } :text

    .got : {
        *(.got)
        *(.got.plt)
    } :text

    .dynamic : {
        *(.dynamic)
    } :text :dynamic

    .bss : {
        *(.bss*)
        *(COMMON)
    } :text

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
