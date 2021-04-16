/*

This linker script:
 - Puts the main .text section with bootloader code at the end of flash.
 - Puts the interrupt vectors at the start of flash (to make the reset
   vector work, and not waste space in the bootloader area for other
   unused interrupt vectors).
 - Puts a trampoline instruction just before the bootloader.

Note that a linker script is really needed in order to separate the
.vectors section from the rest of the text. It seems that linker
commandline options (--section-start, even combined with --unique)
cannot seem to achieve this.

This linker script creatively uses some peculiarities in how ld treats
linker scripts. It is intended to be passed to the -T option on the
commandline. The following happens:
 - The -T option makes ld read this linker script
 - If it finds an "INSERT" statement, it will *also* read the default
   linker script (merging them into a single script, starting with this
   script, followed by the default script).
 - The INSERT statement affects all statement/sections *before* it, and
   inserts them into the right place later in the linker script. In this
   case, it has no effect (but it is needed to let the default script be
   loaded).
 - When an output section occurs twice (such as here and an in the
   default linker script), the entries are merged into a single section.
   Options that are specified twice are taken from the second section,
   but in this case a specific starting address set below overrides the
   implicit starting address derived from the memory region in the
   default script. Any content would be merged together.

Effectively, this means all of the sections defined below are defined at
the start of the linker script, followed by the default linker script.
This also means that things defined by the default script (such as
memory regions) are not defined yet, so we might need to be careful not
to confuse the linker with those).

We could have put some sections above the `INSERT BEFORE .text`
statement, in order to get those section inserted before the .text
section in the output file. However, since the insert really works on
the list of output sections (not the list of statements in the linker
script), this gives all kinds of weirdness. In particular, you cannot
have a .text output section above the `INSERT BEFORE .text` since this
would mean .text needs to be inserted before itself, which fails with
".text not found for insert".

*/


/* This just serves to load the default linker script, it does not
 * actually insert anything. The section can be anything (though it must
 * exist). */
INSERT BEFORE .text;

SECTIONS
{
   /* Calculate the bootloader start point. "text" here refers to the
    * memory region, not the (input or output) section. */
   BL_START = ORIGIN(text) + LENGTH(text) - BL_SIZE;
   VERSION_START = ORIGIN(text) + LENGTH(text) - VERSION_SIZE;

   /* Put the interrupt vector table at the start as normal. This:
    *  - Properly sets the reset vector to jump to the bootloader
    *  - Prevents the other (unused) interrupt vectors from eating up
    *    bootloader flash space. These will be overwritten when an
    *    application is flashed.
    * Putting this section here prevents it from being placed again
    * by the default linker script.
    */
   .text.vectors ORIGIN(text) : {
      reset_vector = .;
      *(.vectors)
   }

   /* Put the boot_trampoline section just before the bootloader.
    * This is a single instruction function that jumps to the
    * application and is updated whenever a new application is
    * flashed. */
   .text.trampoline BL_START - 2 : {
      *(.boot_trampoline)

      /* Verify that the boot_trampoline is the expected size */
      ASSERT(ABSOLUTE(.) == ABSOLUTE(BL_START), "boot_trampoline section should be a single 2-byte instruction");

      /* Verify that the bootloader is aligned to a erase size boundary,
       * so changing boot_trampoline does not need to erase any part of
       * the bootloader. */
      ASSERT(ABSOLUTE(.) % FLASH_ERASE_SIZE == 0, "Bootloader should be aligned to erase size (change BL_SIZE)");
   }

   /* This sets the starting address for the .text section. This output
    * section declaration will be merged with the declaration from the
    * default script, which will provide the contents. */
   .text BL_START : { }

   .text.version VERSION_START : {
      *(.version)
   }
}

/* Keep the entrypoint at 0x0 (but we can only set it through a symbol,
 * not directly). Without this, the entry point is set to the start of
 * the .text section, which does not matter much, but does make a "start
 * address" record (type 03) show up in the hex file. */
ENTRY(reset_vector);
