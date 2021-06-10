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

#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdint.h>
#include <Gpio.h>

#if defined(BOARD_TYPE_interfaceboard)
	const Pin PIN_3V3_ENABLE = {&PORTA, &PINA, &DDRA, &PUEA, 1 << PA2};
	const Pin PIN_BOOST_ENABLE = {&PORTA, &PINA, &DDRA, &PUEA, 1 << PA3};
	const Pin PIN_DISPLAY_RESET = {&PORTB, &PINA, &DDRB, &PUEB, 1 << PB0};

	const uint8_t INFO_HW_TYPE = 1;
	const uint8_t DISPLAY_CONTROLLER_TYPE = 1;
        const uint16_t MAX_PACKET_LENGTH = 32;
	#define HAVE_DISPLAY
	#define NEED_TRAMPOLINE
#elif defined(BOARD_TYPE_gphopper)
	const uint8_t INFO_HW_TYPE = 2;
        const uint16_t MAX_PACKET_LENGTH = 32;
        constexpr const Pin CHILDREN_SELECT_PINS[] = {
            {RCC_GPIOB, GPIOB, GPIO8},
        };
        const Pin CHILD_SELECT_PIN = {RCC_GPIOB, GPIOB, GPIO9};
	#define USE_CHILD_SELECT
#else
	#error "No board type defined"
#endif

#if defined(USE_CHILD_SELECT)
const uint8_t NUM_CHILDREN = sizeof(CHILDREN_SELECT_PINS) / sizeof(*CHILDREN_SELECT_PINS);
#endif

// By default, listen to addresses 8-15
const uint8_t INITIAL_ADDRESS = 0x08;
const uint8_t INITIAL_BITS = 4;

#endif /* CONFIG_H_ */
