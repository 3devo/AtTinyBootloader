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

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include <stdio.h>
#include "../Bus.h"

static uint8_t initAddress = 0;
static uint8_t initMask = 0;
static uint8_t configuredAddress = 0;

static const uint32_t BAUD_RATE = 19200;
static const uint32_t MAX_INTER_FRAME = 1750; // us
static const uint32_t INTER_FRAME_BITS = (MAX_INTER_FRAME * BAUD_RATE + 1e6 - 1) / 1e6;

void BusInit(uint8_t initialAddress, uint8_t initialBits) {
	initAddress = initialAddress;
	// Create a mask with initialBits ones (address bits to match)
	// followed by zeroes (address bits to ignore).
	initMask = ~(0x7f >> initialBits);

	BusResetDeviceAddress();

	/* Setup clocks & GPIO for USART */
	rcc_periph_clock_enable(RCC_USART1);
	rcc_periph_clock_enable(RCC_GPIOB);
	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO3 | GPIO6 | GPIO7);
	// RX & TX
	gpio_set_af(GPIOB, GPIO_AF0, GPIO6 | GPIO7);
	// RTS/DE
	gpio_set_af(GPIOB, GPIO_AF3, GPIO3);

	/* Setup USART parameters. */
	usart_set_baudrate(USART1, BAUD_RATE);
	usart_set_databits(USART1, 8+1); // Includes parity bit
	usart_set_parity(USART1, USART_PARITY_EVEN);
	usart_set_stopbits(USART1, USART_CR2_STOPBITS_1);
	usart_set_mode(USART1, USART_MODE_TX_RX);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

	usart_set_rx_timeout_value(USART1, INTER_FRAME_BITS);
	usart_enable_rx_timeout(USART1);

	// Enable Driver Enable on RTS pin
	USART_CR3(USART1) |= USART_CR3_DEM;

	/* Finally enable the USART. */
	usart_enable(USART1);
}

void BusDeinit() {
	rcc_periph_reset_pulse(RST_USART1);

	gpio_mode_setup(GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO3 | GPIO6 | GPIO7);
	gpio_set_af(GPIOB, GPIO_AF0, GPIO3 | GPIO6 | GPIO7);

	rcc_periph_clock_disable(RCC_USART1);
	rcc_periph_clock_disable(RCC_GPIOB);
}

void BusSetDeviceAddress(uint8_t address) {
	// Enable single address
	configuredAddress = address;
}

void BusResetDeviceAddress() {
	configuredAddress = 0;
}

#define BUFFER_SIZE 32
static uint8_t busBuffer[BUFFER_SIZE];
static uint8_t busBufferLen = 0;
static uint8_t busTxPos = 0;
static uint8_t busAddress = 0;

enum State {
	StateIdle,
	StateRead,
	StateWrite
};

static State busState = StateIdle;

static bool matchAddress(uint8_t address) {
	if (address == 0) // General call
		return true;
	if (configuredAddress)
		return address == configuredAddress;
	return (address & initMask) == initAddress;
}

void BusUpdate() {
	// Uncomment this to enable debug prints in this function
	// Breaks Rs485 communication unless debug baudrate is 20x or so
	// faster than Rs485!
	#define printf(...) do {} while(0)
	uint32_t isr = USART_ISR(USART1);

	/*
	static uint32_t prev_isr = 0;
	if (isr != prev_isr) {
		printf("isr: 0x%lx\n", isr);
		prev_isr = isr;
	}
	*/


	if (isr & USART_ISR_TXE && busState == StateWrite) {
		// TX register empty, writing data clears TXE
		printf("tx: %02x\n", (unsigned)busBuffer[busTxPos]);
		usart_send(USART1, busBuffer[busTxPos++]);
		if (busTxPos >= busBufferLen)
			busState = StateIdle;
		// TODO: Clear error flags and/or RTOF after TX?
	} else if (isr & USART_ISR_RXNE && busState != StateWrite) { // Received data

		// Reading data clears RXNE
		uint8_t data = usart_recv(USART1);
		printf("rx: 0x%02x\n", (unsigned)data);

		if (busState == StateIdle) {
			busAddress = data;
			busState = StateRead;
			busBufferLen = 0;
		} else if (busBufferLen < BUFFER_SIZE) {
			busBuffer[busBufferLen++] = data;
		} else {
			printf("rx ovf\n");
		}
	}
	if ((isr & (USART_ISR_RTOF)) && busState == StateRead) {
		// Sufficient silence after last RX byte
		printf("rxtimeout after %u bytes\n", busBufferLen);

		// This checks for errors that occurred during any byte
		// in the transfer
		bool rxok = true;
		if (isr & USART_ISR_PE) {
			printf("parity error during transfer\n");
			rxok = false;
		}
		if (isr & USART_ISR_FE ) {
			printf("framing error during transfer\n");
			rxok = false;
		}
		if (isr & USART_ISR_ORE) {
			printf("overrun error during transfer\n");
			rxok = false;
		}

		// Clear timeout flag and any errors
		USART_ICR(USART1) = USART_ICR_RTOCF | USART_ICR_PECF | USART_ICR_FECF | USART_ICR_ORECF;

		bool matched = matchAddress(busAddress);
		printf("address 0x%x %smatched\n", busAddress, matched ? "" : "not ");

		// RX addressed to us, execute the callback and setup for a read.
		if (!rxok || busBufferLen == 0 || !matched) {
			busBufferLen = 0;
		} else {
			busBufferLen = BusCallback(busAddress, busBuffer, busBufferLen, BUFFER_SIZE);
		}
		if (busBufferLen) {
			busState = StateWrite;
			busTxPos = 0;
		} else {
			busState = StateIdle;
		}
	}
	#undef printf
}
