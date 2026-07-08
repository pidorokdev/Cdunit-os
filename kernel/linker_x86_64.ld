/*
 * Dunit OS Kernel Linker Script
 * x86_64 (64-bit) kernel linking configuration
 */

/* Entry point */
ENTRY(_start)

/* Kernel physical load address */
KERNEL_LMA = 0x100000;            /* 1MB */

SECTIONS
{
    /* Start at 1MB physical address */
    . = KERNEL_LMA;
    
    /* Keep track of kernel start */
    __kernel_start = .;
    __kernel_phys_start = .;
    
    /* =================================================================== */
    /* Code Section (.text) */
    /* =================================================================== */
    .notes ALIGN(4) : {
        KEEP(*(.note.Xen))
        *(.note*)
    }

    .text ALIGN(4K) : {
        __text_start = .;
        
        /* Boot code must come first */
        *(.text.boot)
        
        /* Then the rest of the kernel code */
        *(.text .text.*)
        
        __text_end = .;
    }
    
    /* =================================================================== */
    /* Read-Only Data Section (.rodata) */
    /* =================================================================== */
    .rodata ALIGN(4K) : {
        __rodata_start = .;
        
        *(.rodata .rodata.*)
        
        __rodata_end = .;
    }
    
    /* =================================================================== */
    /* Data Section (.data) */
    /* =================================================================== */
    .data ALIGN(4K) : {
        __data_start = .;
        
        *(.data .data.*)
        
        __data_end = .;
    }
    
    /* =================================================================== */
    /* BSS Section (Uninitialized Data) */
    /* =================================================================== */
    .bss ALIGN(4K) : {
        __bss_start = .;
        
        *(.bss .bss.*)
        *(COMMON)
        
        __bss_end = .;
    }
    
    /* =================================================================== */
    /* Kernel Stack */
    /* =================================================================== */
    . = ALIGN(16);
    __stack_bottom = .;
    . += 64K;  /* 64KB kernel stack */
    __stack_top = .;
    
    /* =================================================================== */
    /* End of Kernel */
    /* =================================================================== */
    . = ALIGN(4K);
    __kernel_end = .;
    __kernel_phys_end = .;
    
    /* Calculate kernel size */
    __kernel_size = __kernel_end - __kernel_start;
    
    /* =================================================================== */
    /* Debug Sections (stripped in release) */
    /* =================================================================== */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame*)
    }
}
