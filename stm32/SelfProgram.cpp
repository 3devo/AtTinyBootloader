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

#include <libopencm3/stm32/flash.h>
#include "../SelfProgram.h"

#if !defined(FLASH_BASE)
// https://github.com/libopencm3/libopencm3-examples/issues/224
#define FLASH_BASE                      (0x08000000U)
#endif

// Writing happens per doubleword
#if FLASH_WRITE_SIZE != 8
#error "Incorrect FLASH_WRITE_SIZE"
#endif

#if FLASH_ERASE_SIZE % FLASH_WRITE_SIZE != 0
#error "Incorrect FLASH_ERASE_SIZE"
#endif

#if FLASH_APP_OFFSET % FLASH_ERASE_SIZE != 0
#error "Incorrect FLASH_APP_OFFSET"
#endif

uint8_t SelfProgram::eraseCount = 0;

void SelfProgram::readFlash(uint16_t address, uint8_t *data, uint16_t len) {
	for (uint8_t i=0; i < len; i++) {
		data[i] = readByte(address + i);
	}
}

uint8_t SelfProgram::readByte(uint16_t address) {
	uint8_t *ptr = (uint8_t*)FLASH_BASE + FLASH_APP_OFFSET + address;
	return *ptr;
}

uint8_t SelfProgram::writePage(uint16_t address, uint8_t *data, uint16_t len) {
	// Can only write to a 16 byte page boundary
	if (!len || address % FLASH_WRITE_SIZE != 0 || len > FLASH_WRITE_SIZE) {
		return 1;
	}

	// If the address is past the application section don't write anything
	if (address + len > applicationSize) {
		return 3;
	}

	flash_unlock();
	flash_clear_status_flags();

	// If we are the beginning of a page, erase it
	if (address % FLASH_ERASE_SIZE == 0) {
		if (eraseCount < 0xff)
			++eraseCount;
		flash_erase_page((address + FLASH_APP_OFFSET) / FLASH_ERASE_SIZE);
	}

	// If no errors from erase, then program
	if (FLASH_SR == 0) {
		// TODO: There is a "fast programming mode", that writes
		// 256 bytes at a time, but it seems opencm3 does not
		// support this yet.
		uint64_t value =
			(uint64_t)data[0] << 0 |
			(uint64_t)data[1] << 8 |
			(uint64_t)data[2] << 16 |
			(uint64_t)data[3] << 24 |
			(uint64_t)data[4] << 32 |
			(uint64_t)data[5] << 40 |
			(uint64_t)data[6] << 48 |
			(uint64_t)data[7] << 56;
		flash_program_double_word(FLASH_BASE + FLASH_APP_OFFSET + address, value);
	}
	flash_lock();


	// Get and clear error flags
	uint32_t errbits = FLASH_SR & 0xffff;
	FLASH_SR = 0xffff;

	// This uses the lower nibble to indicate the first
	// (least-significant) error bit set, and the upper nibble to
	// indicate the number of error bits set (just in case there are
	// multiple). For example, if just PROGERR (bit 3) is set, this
	// would result in 0x13. This leaves values < 16 free to be used
	// above.
	uint8_t res = 0;
	while (errbits) {
		if (errbits & 1)
			res += 0x10;
		else if ((res & 0xf0) == 0)
			++res;
		errbits >>= 1;
	}
	return res;
}
