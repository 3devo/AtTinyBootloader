/*
 * Copyright (C) 2015-2017 Erin Tomson <erin@rgba.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// This is the Modulo bootloader for attiny841 based devices.
// By default, the bootloader uses the following memory layout (but this
// can be changed by changing BL_SIZE in the Makefile):
//
//   0x0000 - 2 byte reset vector. Jumps to the bootloader section (0x1800)
//   0x17FE - 2 byte application trampoline. The bootloader will jump here when it exits.
//   0x1800 - 2048 byte bootloader
//
// The bootloader will write bytes 0x0002-0x17FD, protecting the reset
// vector, trampoline and bootloader code. The reset vector will be
// automatically copied into the trampoline area.
//
// To view the disassembled bootloader, run:
//  /cygdrive/c/Program\ Files\ \(x86\)/Atmel/Atmel\ Toolchain/AVR8\ GCC/Native/3.4.1061/avr8-gnu-toolchain/bin/avr-objdump.exe -d Release/bootloader-attiny.elf


#if defined(__AVR__)
#include <avr/io.h>
#include <avr/wdt.h>
#elif defined(STM32)
#include <libopencm3/cm3/scb.h>
#endif
#include "bootloader.h"
#include "SelfProgram.h"
#include <stdio.h>

#if defined(__AVR_ATtiny841__) || defined(__AVR_ATtiny441__)
// This is a single instruction placed immediately before the bootloader.
// The application code will overwrite this instruction with a jump to to the start of the application.
void __attribute__((noreturn)) __attribute__((noinline)) __attribute__((naked)) __attribute__((section(".boot_trampoline"))) startApplication();
void startApplication() {
	// When no application is flashed yet, this code is used. This
	// just restarts the bootloader.
	asm("rjmp __vectors");
	__builtin_unreachable();
}

// Store the fuse bits in a separate section of the elf file.
// Note that fuse bits are inverted (0 enables the feature) so we must bitwise
// and the masks together.
FUSES =
{
	// Internal 8Mhz, 6CK / 14CK+16ms startup time
	.low = FUSE_SUT_CKSEL4 & FUSE_SUT_CKSEL3 & FUSE_SUT_CKSEL2 & FUSE_SUT_CKSEL0,
	// Brown-out at 2.5-2.9V (8Mhz needs 2.4V or more)
	.high = FUSE_SPIEN & FUSE_EESAVE & FUSE_BODLEVEL1,
	// BOD always on
	.extended = FUSE_SELFPRGEN & FUSE_BODACT0 & FUSE_BODPD0
};
#elif defined(STM32)

#if !defined(FLASH_BASE)
// https://github.com/libopencm3/libopencm3-examples/issues/224
#define FLASH_BASE                      (0x08000000U)
#endif

#if defined(STM32)
extern "C" {
	// Ensure that any faults reset the system, rather than blocking
	// in a loop. This ensures the board can be addressed even when
	// a fault happens
	void hard_fault_handler() { scb_reset_system(); }
	void mem_manage_handler() __attribute__((alias("hard_fault_handler")));
	void bus_fault_handler() __attribute__((alias("hard_fault_handler")));
	void usage_fault_handler() __attribute__((alias("hard_fault_handler")));
}
#endif

void startApplication() __attribute__((__noreturn__));
void startApplication() {
	const uint32_t *application = (uint32_t*)(FLASH_BASE + FLASH_APP_OFFSET);
	const uint32_t top_of_stack = application[0];
	const uint32_t reset_vector = application[1];

	asm("msr msp, %0; bx %1;" : : "r"(top_of_stack), "r"(reset_vector));
	__builtin_unreachable();
}
#else
#error "Unsupported arch"
#endif

extern void uart_init();

int main() {
	#if defined(__AVR__)
	// Disable watchdog, to prevent it triggering again
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
	#endif // defined(__AVR__)

	#if defined(NEED_TRAMPOLINE)
	// Set this value here, to avoid gcc generating a lot of
	// overhead for running a constructor to set this value. It
	// cannot be inline at compiletime, since the value is not known
	// until link time.
	SelfProgram::trampolineStart = (uint16_t)&startApplication * 2;
	#endif // defined(NEED_TRAMPOLINE)

	// Uncomment this to allow the use of printf on pin PA1 (ATTiny)
	// or PA2 (stm32), TX only. See also usart.cpp. This needs a
	// bigger bootloader area.
	//uart_init();
	//printf("Hello\n");

	runBootloader();
	startApplication();
}
