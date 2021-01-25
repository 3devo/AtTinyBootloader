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

#ifndef SELFPROGRAM_H_
#define SELFPROGRAM_H_

#include <stdint.h>

void startApplication();

class SelfProgram {
public:
	static void readFlash(uint16_t address, uint8_t *data, uint8_t len);

	static uint8_t readByte(uint16_t address);

	static uint8_t writePage(uint16_t address, uint8_t *data, uint8_t len);

	static void writeTrampoline(uint16_t instruction);

	static uint16_t offsetRelativeJump(uint16_t instruction, int16_t offset);

	static uint16_t trampolineStart;

	// Use a reference to make this an alias to trampolineStart for
	// readability
	static constexpr const uint16_t& applicationSize = trampolineStart;

	static uint8_t eraseCount;
};

#endif /* SELFPROGRAM_H_ */
