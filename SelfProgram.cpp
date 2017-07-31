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

#define SIGRD 5

#include "SelfProgram.h"
#include "bootloader.h"

#include <avr/io.h>
#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

SelfProgram::SelfProgram() {
}

uint32_t SelfProgram::getSignature() {
	return (boot_signature_byte_get(0) |
			(((uint32_t)boot_signature_byte_get(0)) << 8) |
			(((uint32_t)boot_signature_byte_get(0)) << 16));
}

void SelfProgram::readEEPROM(uint16_t address, uint8_t *data,  uint8_t eeLen) {
	eeprom_read_block(data, (const void *)address, eeLen);
}

void SelfProgram::writeEEPROM(uint16_t address, uint8_t *data,  uint8_t eeLen) {
	eeprom_update_block(data, (void*)address, eeLen);
}

// Flash page size is 16 bytes
int SelfProgram::getPageSize() {
	return SPM_PAGESIZE;
}

void SelfProgram::erasePage(uint16_t address) {
	// Mask of bits inside the page
	address &= ~(SPM_ERASESIZE - 1);

	// Bail if the address is past the application section.
	if (address + SPM_ERASESIZE > applicationSize) {
		return;
	}

	boot_page_erase_safe(address);

	// If we just erased page 0, restore the reset vector in case
	// the bootloader gets interrupted before writing the page.
	// writePage() will change bytes 0 and 1.
	// All the other bytes in the page must remain erased (0xFF)
	if (address == 0) {
		uint8_t data[SPM_PAGESIZE];
		for (int i=0; i < SPM_PAGESIZE; i++) {
			data[i] = 0xFF;
		}
		writePage(0, data, SPM_PAGESIZE);
	}

	boot_spm_busy_wait();
}

void SelfProgram::readFlash(uint16_t address, uint8_t *data, uint8_t len) {
	for (uint8_t i=0; i < len; i++) {
		data[i] = readByte(address + i);
	}
}

uint8_t SelfProgram::readByte(uint16_t address) {
	// The first two bytes have been relocated to the end of flash,
	// so read from there, and make sure to undo the changes made
	if (address < 2) {
		uint16_t instruction = pgm_read_word(trampolineStart);
		instruction = offsetRelativeJump(instruction, trampolineStart);
		return address == 0 ? instruction : (instruction >> 8);
	}
	return pgm_read_byte(address);
}

uint8_t SelfProgram::writePage(uint16_t address, uint8_t *data, uint8_t len) {
	// Can only write to a 16 byte page boundary
	if (!len || address % SPM_PAGESIZE != 0 || len > SPM_PAGESIZE) {
		return 1;
	}

	// If we are writing page 0, change the reset vector
	// so that it jumps to the bootloader.
	if (address == 0) {
		// Copy the reset vector from the data into the boot
		// trampoline area
		uint16_t instruction = data[0] | (data[1] << 8);
		instruction = offsetRelativeJump(instruction, -trampolineStart);
		// Not a supported instruction? Return an error
		if (!instruction)
			return 2;

		// Write the to-be-written reset vector to the trampoline
		writeTrampoline(instruction);

		// And preserve the current reset vector
		data[0] = pgm_read_byte(0);
		data[1] = pgm_read_byte(1);
	}

	// If the address is past the application section don't write anything
	if (address + len > applicationSize) {
		return 3;
	}

	// If we are the beginning of a 4-page boundary, erase it
	if (address % SPM_ERASESIZE == 0) {
		// If this is the page containing the trampoline, it
		// will already be erased when writing the first page
		if (address / SPM_ERASESIZE != trampolineStart / SPM_ERASESIZE)
			boot_page_erase_safe(address);
	}

	for (int i=0; i < len; i += 2) {
		uint16_t w = data[i] | (data[i+1] << 8);
		boot_page_fill_safe(address+i, w);
	}
	boot_page_write_safe(address);

	return 0;
}

// This expects a rjmp or rcall instruction, which stores the
// jump amount (relative to the current address) in the lower 12 bits.
// The given offset (in bytes) is added to the jump amount.
uint16_t SelfProgram::offsetRelativeJump(uint16_t instruction, int16_t offset) {
	if ((instruction & 0xE000) != 0xC000)
		return 0;

	// Since flash sizes are always a power of two, these jumps wrap
	// cleanly and any extra bits (e.g. when flash is < 2**12 words
	// long) are ignored. This also means we do not really have to
	// distinguish between a positive or negative jump, the wrap
	// around makes sure that the used bits will be the same anyway.
	uint16_t jump = instruction & 0xFFF;
	// Jump amount is in words, not bytes
	jump += (offset / 2);
	return (instruction & 0xF000) | (jump & 0xFFF);
}

void SelfProgram::writeTrampoline(uint16_t instruction) {
	uint16_t address = trampolineStart;
	// Erase the page containing the trampoline. This might erase
	// other application code. This makes no attempt to preserve the
	// other code to keep since simple. This should work since we're
	// writing a new application anyway.
	boot_page_erase_safe(address);

	// Take the reset instruction from the page at offset 0 and 1
	// and overwrite the trampoline with it
	boot_page_fill_safe(address, instruction);
	boot_page_write_safe(address);
}
