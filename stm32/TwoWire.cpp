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
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/gpio.h>
#include <stdio.h>
#include "../Bus.h"

static uint8_t initAddress = 0;
static uint8_t initBits = 0;

void BusInit(uint8_t initialAddress, uint8_t initialBits) {
	initAddress = initialAddress;
	initBits = initialBits;

	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_I2C1);

	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO6 | GPIO7);
	gpio_set_af(GPIOB, GPIO_AF6, GPIO6 | GPIO7);
	gpio_set_output_options(GPIOB, GPIO_OTYPE_OD,  GPIO_OSPEED_2MHZ, GPIO6 | GPIO7);

	i2c_reset(I2C1);
	i2c_peripheral_disable(I2C1);

	i2c_enable_analog_filter(I2C1);
	i2c_set_digital_filter(I2C1, 0);

	/* HSI is at 16Mhz */
	i2c_set_speed(I2C1, i2c_speed_sm_100k, 16);
	i2c_enable_stretching(I2C1);

	BusResetDeviceAddress();

	//addressing mode
	i2c_set_7bit_addr_mode(I2C1);
	i2c_peripheral_enable(I2C1);
}

void BusDeinit() {
	i2c_reset(I2C1);

	gpio_mode_setup(GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO6 | GPIO7);
	gpio_set_af(GPIOB, GPIO_AF0, GPIO6 | GPIO7);

	rcc_periph_clock_disable(RCC_I2C1);
	rcc_periph_clock_disable(RCC_GPIOB);
}

void BusSetDeviceAddress(uint8_t address) {
	// Enable single address
	// TODO: If this is done when a start condition and/or address
	// has already been received (i.e. with repeated start or a read
	// following a stop closely), the I2C hardware seems to get
	// confused and keeps pulling the data line low.
	I2C_OAR2(I2C1) &= ~I2C_OAR2_OA2EN;
	I2C_OAR2(I2C1) = I2C_OAR2_OA2EN | (address << 1);
}

void BusResetDeviceAddress() {
	// Enable general call
	I2C_CR1(I2C1) |= I2C_CR1_GCEN;

	// Enable masked address
	uint8_t oa2msk = (7 - initBits);
	I2C_OAR2(I2C1) &= ~I2C_OAR2_OA2EN;
	I2C_OAR2(I2C1) = I2C_OAR2_OA2EN | (oa2msk << 8) | (initAddress << 1);
}

static uint8_t twiBuffer[MAX_PACKET_LENGTH];
static uint8_t twiBufferLen = 0;
static uint8_t twiReadPos = 0;
static uint8_t twiAddress = 0;
static bool isReadOperation;
static_assert(MAX_PACKET_LENGTH < (1 << (sizeof(twiBufferLen) * 8)), "Code needs changes for bigger packets");

// Extract the address from the I²C status register
uint8_t address_from_isr(uint32_t isr) {
	// ADDCODE field, not defined by libopencm3 yet
	return (isr >> 17) & 0x7f;
}

enum TWIState {
	TWIStateIdle,
	TWIStateRead,
	TWIStateWrite
};

static TWIState twiState = TWIStateIdle;

void BusUpdate() {
	// Uncomment this to enable debug prints in this function
	#define printf(...) do {} while(0)
	uint32_t isr = I2C_ISR(I2C1);
	if (isr & (I2C_ISR_RXNE|I2C_ISR_TXIS|I2C_ISR_STOPF|I2C_ISR_ADDR))
		printf("isr: 0x%lx, %lx\n", isr, I2C_CR2(I2C1));

	if (isr & I2C_ISR_RXNE) { // Received data
		// Reading data clears RXNE
		uint8_t data = i2c_get_data(I2C1);

		if (twiBufferLen < sizeof(twiBuffer))
			twiBuffer[twiBufferLen++] = data;
		printf("rx: 0x%02x\n", (unsigned)data);
	}
	if (isr & I2C_ISR_TXIS) {
		// TX byte needed
		// writing data clears TXIS
		//i2c_send_data(I2C1, *write_p--);
		if (twiReadPos < twiBufferLen) {
			printf("tx: %02x\n", (unsigned)twiBuffer[twiReadPos]);
			i2c_send_data(I2C1, twiBuffer[twiReadPos++]);
		} else {
			printf("tx ovf\n");
			// Send dummy data
			i2c_send_data(I2C1, 0);
		}
	}
	// This runs on STOPF but also on ADDR to handle repeated start
	if ((isr & (I2C_ISR_STOPF|I2C_ISR_ADDR)) && twiState == TWIStateWrite) {
		printf("stop|repstart\n");
		// If we were previously in a write, then execute the callback and setup for a read.
		if (twiBufferLen != 0)
			twiBufferLen = BusCallback(twiAddress, twiBuffer, twiBufferLen, sizeof(twiBuffer));
		twiState = TWIStateIdle;
	}
	// Clear stop flag
	if (isr & I2C_ISR_STOPF)
		I2C_ICR(I2C1) = I2C_ICR_STOPCF;

	if (isr & I2C_ISR_ADDR) { // Address matched
		// Save this to a global, since we need it when
		// processing a stop, and by then the ISR register might
		// have already changed (if the stop is directly
		// followed by a new transaction).
		isReadOperation = (isr & I2C_ISR_DIR_READ);

		// Send an ack unless a read is starting and there are no bytes to read.
		bool ack = (twiBufferLen > 0) || (!isReadOperation);
		if (!ack) {
			// TODO: It seems setting CR2_NACK does not
			// prevent the address from being ACK'd (only
			// applies to data bytes), nor does clearing
			// OA2EN at this point. So just disable and
			// re-enable the entire I2C hardware to prevent
			// an ack, which is a bit of a hack...
			//I2C_CR1(I2C1) |= I2C_CR2_NACK;
			I2C_CR1(I2C1) &= ~I2C_CR1_PE;
			while(I2C_CR1(I2C1) & I2C_CR1_PE) /* wait */;
			I2C_CR1(I2C1) |= I2C_CR1_PE;
		}

		if (isReadOperation)
			twiReadPos = 0;
		else
			twiBufferLen = 0;

		// The address is in the high 7 bits, the RD/WR bit is in the lsb
		twiAddress = address_from_isr(isr);
		printf("addr: 0x%02x (%c, %s)\n", (unsigned)twiAddress, isReadOperation ? 'r' : 'w', ack ? "ack" : "nak");

		// Force TXE to flush any TX data still leftover from a
		// previous transaction
		I2C_ISR(I2C1) = I2C_ISR_TXE;

		// Acknowledge address
		I2C_ICR(I2C1) = I2C_ICR_ADDRCF;
		twiState = isReadOperation ? TWIStateRead : TWIStateWrite;
	}
	#undef printf
}
