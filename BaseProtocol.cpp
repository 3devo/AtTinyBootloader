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

static int handleGeneralCall(uint8_t *data, uint8_t len, uint8_t /* maxLen */) {
	if (len >= 1 && data[0] == GeneralCallCommands::RESET) {
		#if defined(__AVR__)
		wdt_enable(WDTO_15MS);
		while(true) /* wait */;
		#elif defined(STM32)
		scb_reset_system();
		#else
		#error "Unsupported arch"
		#endif

	} else if (len >= 1 && data[0] == GeneralCallCommands::RESET_ADDRESS) {
		BusResetDeviceAddress();
	}
	return 0;
}

int BusCallback(uint8_t address, uint8_t *data, uint8_t len, uint8_t maxLen) {
	if (address == 0)
		return handleGeneralCall(data, len, maxLen);

	// Check that there is at least room for a status byte and a CRC
	if (maxLen < 2)
		return 0;

	if (len < 2) {
		data[0] = Status::INVALID_TRANSFER;
		len = 1;
	} else {
		uint8_t crc = Crc8Ccitt().update(data, len).get();
		if (crc != 0) {
			data[0] = Status::INVALID_CRC;
			len = 1;
		} else {
			// CRC checks out, process a command
			cmd_result res = processCommand(data[0], data + 1, len - 2, data + 2, maxLen - 3);
			if (res.status == Status::NO_REPLY)
				return 0;

			data[0] = res.status;
			len = res.len + 1;
		}
	}

	data[1] = len - 1;
	++len;

	data[len] = Crc8Ccitt().update(data, len).get();
	++len;


	return len;
}


