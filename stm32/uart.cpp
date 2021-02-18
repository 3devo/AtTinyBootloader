/*
 * Copyright (C) 2021 (http://www.3devo.eu)
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

#define _GNU_SOURCE 1 // For fopencookie

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include <stdio.h>

#define BAUD 1000000

static ssize_t uart_putchar (void *, const char *buf, size_t len) {
	const char *end = buf + len;
	while (buf != end)
		usart_send_blocking(USART2, *buf++);
	return len;
}

static cookie_io_functions_t functions = {
	.read = NULL,
	.write = uart_putchar,
	.seek = NULL,
	.close = NULL
};

void uart_init(void) {
	/* Setup clocks & GPIO for USART */
	rcc_periph_clock_enable(RCC_USART2);
	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2);
	gpio_set_af(GPIOA, GPIO_AF1, GPIO2);

	/* Setup USART2 parameters. */
	usart_set_baudrate(USART2, BAUD);
	usart_set_databits(USART2, 8);
	usart_set_parity(USART2, USART_PARITY_NONE);
	usart_set_stopbits(USART2, USART_CR2_STOPBITS_1);
	usart_set_mode(USART2, USART_MODE_TX);
	usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);

	/* Finally enable the USART. */
	usart_enable(USART2);

	/* Setup stdout for printf. This is a GNU-specific extension to libc. */
	stdout = fopencookie(NULL, "w", functions);
	/* Disable buffering, so the callbacks get called right away */
	setbuf(stdout, nullptr);
}
