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

#ifndef BOARDS_H_
#define BOARDS_H_

struct Pin {
	volatile uint8_t* port;
	volatile uint8_t* ddr;
	volatile uint8_t* pue;
	uint8_t mask;
};

#if defined(BOARD_TYPE_interfaceboard)
	Pin PIN_3V3_ENABLE = {&PORTA, &DDRA, &PUEA, 1 << PA2};
	Pin PIN_BOOST_ENABLE = {&PORTA, &DDRA, &PUEA, 1 << PA3};
	Pin PIN_DISPLAY_RESET = {&PORTB, &DDRB, &PUEB, 1 << PB0};

	const uint8_t INFO_HW_TYPE = 1;
	const uint8_t DISPLAY_CONTROLLER_TYPE = 1;
	#define HAVE_DISPLAY
#else
	#error "No board type defined"
#endif

// By default, listen to addresses 8-15
const uint8_t INITIAL_I2C_ADDRESS = 0x08;
const uint8_t INITIAL_I2C_MASK = 0x07;

#endif /* BOARDS_H_ */
