Childbus bootloader
===================
This is a bootloader intended for attiny or STM32G0 devices, accessible
through an I²C or RS485 bus.

It was developed by 3devo for their Filament Extruder to support a
system with a single master controller and one or more supporting child
controllers.
 - At poweron, a child starts the bootloader. This child listens on the
   bus for commands for the master. There is no timeout, so the
   bootloader will just wait if the master does not send any info.
 - The master detects the child and uploads an application to it. This
   happens again on every boot (though the child takes care to prevent a
   flash erase and write cycle when the data is identical).
 - The master tells the child to start the application.

Attiny support
--------------
Since the attinies have no native bootloader support (the reset vector
always resets to address 0x0), the bootloader automatically overwrites
the reset vector with its own start address and saves the original reset
vector in a separate area of flash (just before the bootloader start).

This code only supports the attiny841 and attiny441 (only 841 was
tested) with TWI hardware. Some attempts have been made to also support
attiny24/44/84 with USI hardware, which worked (see the two-wire-usi
branch), but due to some of the polling required this was a bit less
robust.

The bootloader needs about 2k of flash.

STM32 support
-------------
This currently supports the STM32G030, but should not be too hard to
port to other STM32G0 or other STM32 chips.

This chip has no native bootloader support, but instead of overwriting
the reset vector, like with the attiny, the bootloader is written at the
start of flash (so the chip starts the bootloader on reset) and the
application firmware is written after the bootloader. This does require
that the application firmware is compiled with a proper address offset,
and that it relocates the interrupt vector table if it needs interrupts.

The bootloader needs 4k of flash currently (in reality just over 2k, but
rounded up to full 2k erase pages).

License
-------
The bootloader is based on the bootloader written by Erin Tomson for the
Modulo board. Mostly the TWI implementation remains, most of the other
parts of the bootloader have been replaced.

Most code and accompanying materials is licensed under the GPL, version
3. See the LICENSE file for the full license.

The protocol documentation, in the PROTOCOL.md file, has a more liberal
license (see the file for the exact license). The testing sketch, in the
BootloaderTest directory, is licensed under the 3-clause BSD license.
