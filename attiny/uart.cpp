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

#include <avr/io.h>
#include <stdio.h>

#define BAUD 1000000
#include <util/setbaud.h>

int uart_write(char byte, FILE *) {
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = byte;
	return 0;
}


void uart_init(void) {
	UBRR0H = UBRRH_VALUE;
	UBRR0L = UBRRL_VALUE;
	#if USE_2X
	UCSR0A |= (1 << U2X0);
	#else
	UCSR0A &= ~(1 << U2X0);
	#endif

	UCSR0B = (1<<TXEN0);
	UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);
	static FILE stream;
	fdev_setup_stream(&stream, uart_write, NULL, _FDEV_SETUP_WRITE);

	stdout = &stream;
}
