/*
 * Copyright (C) 2017 3devo (http://www.3devo.eu)
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

#include <stdint.h>
#if defined(__AVR__)
#include <avr/wdt.h>
#elif defined(STM32)
#include <libopencm3/cm3/scb.h>
#endif
#include "Bus.h"
#include "Crc.h"
#include "BaseProtocol.h"
#include "Boards.h"

static int handleGeneralCall(uint8_t *data, uint8_t len, uint8_t /* maxLen */) {
	if (len == 1 && data[0] == GeneralCallCommands::RESET) {
		#if defined(__AVR__)
		wdt_enable(WDTO_15MS);
		while(true) /* wait */;
		#elif defined(STM32)
		scb_reset_system();
		#else
		#error "Unsupported arch"
		#endif

	} else if (len == 1 && data[0] == GeneralCallCommands::RESET_ADDRESS) {
		BusResetDeviceAddress();
	}
	return 0;
}

#if defined(USE_I2C)
	int BusCallback(uint8_t address, uint8_t *data, uint8_t len, uint8_t maxLen) {
		if (address == 0)
			return handleGeneralCall(data, len, maxLen);

		// Check that there is at least room for a status, length and a CRC
		if (maxLen < 3)
			return 0;

		cmd_result res(0);
		// Check we received at least command and crc
		if (len < 2) {
			res = cmd_result(Status::INVALID_TRANSFER);
		} else {
			uint8_t crc = Crc8Ccitt().update(data, len).get();
			if (crc != 0) {
				res = cmd_result(Status::INVALID_CRC);
			} else {
				// CRC checks out, process a command
				res = processCommand(data[0], data + 1, len - 2, data + 2, maxLen - 3);
				if (res.status == Status::NO_REPLY)
					return 0;
			}
		}

		data[0] = res.status;
		data[1] = res.len;
		len = res.len + 2;

		uint8_t crc = Crc8Ccitt().update(data, len).get();
		data[len++] = crc;

		return len;
	}
#elif defined(USE_RS485)
	int BusCallback(uint8_t address, uint8_t *data, uint8_t len, uint8_t maxLen) {
		// Check that there is at least room for a status, length and a CRC
		// Check that there is at least room for an address, status, length and CRC
		if (maxLen < 5)
			return 0;

		cmd_result res(0);
		// Check we received at least command and crc
		if (len < 3) {
			res = cmd_result(Status::INVALID_TRANSFER);
		} else {
			uint16_t crc = Crc16Ibm().update(address).update(data, len - 2).get();
			if (crc != (data[len - 2] | data[len - 1] << 8)) {
				// Invalid CRC, so no reply (we cannot
				// be sure that the message was really
				// for us, some someone else might also
				// reply).
				return 0;
			} else if (address == 0) {
				return handleGeneralCall(data, len - 2, maxLen);
			} else {
				// CRC checks out, process a command
				res = processCommand(data[0], data + 1, len - 3, data + 3, maxLen - 5);
				if (res.status == Status::NO_REPLY)
					return 0;
			}
		}

		data[0] = address;
		data[1] = res.status;
		data[2] = res.len;
		len = res.len + 3;

		uint16_t crc = Crc16Ibm().update(data, len).get();
		data[len++] = crc;
		data[len++] = crc >> 8;

		return len;
	}
#endif
