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
#include "../Config.h"
#if defined(USE_LL_HAL)
#include <stm32yyxx_ll_gpio.h>
#include <stm32yyxx_ll_usart.h>
#include <stm32yyxx_ll_bus.h>
#else
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#endif
#include <stdio.h>
#include "../Bus.h"

#if defined(USE_LL_HAL)
	// Compatibility macros to run on ST LL HAL (e.g. inside STM32
	// arduino core) rather than libopencm3. This is not intended as
	// a complete and generic compatibility layer, but just minimal
	// things to make the below code work.
	#define USART_ICR(instance) (instance->ICR)
	#define USART_ISR(instance) (instance->ISR)
	#define USART_CR1(instance) (instance->CR1)
	#define USART_CR3(instance) (instance->CR3)
	#define usart_enable LL_USART_Enable
	#define USART_PARITY_EVEN LL_USART_PARITY_EVEN
	#define usart_set_parity LL_USART_SetParity
	#define usart_enable_rx_timeout LL_USART_EnableRxTimeout
	#define usart_set_rx_timeout_value LL_USART_SetRxTimeout
	#define USART_MODE_TX_RX 0
	#define usart_set_mode(instance, mode) do {LL_USART_EnableDirectionTx(instance); LL_USART_EnableDirectionRx(instance); } while(0)
	// This assumes default ABP clock
	#define usart_set_baudrate(instance, baud) LL_USART_SetBaudRate(instance, 16000000, LL_USART_PRESCALER_DIV1, LL_USART_OVERSAMPLING_16, baud)
	#define RCC_GPIOA 0
	#define RCC_USART1 LL_APB2_GRP1_PERIPH_USART1
	#define rcc_periph_clock_enable(clk) do { \
		if (clk == RCC_GPIOA) LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA); \
		if (clk == RCC_USART1) LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1); \
	} while (0)
	#define rcc_periph_clock_disable(clk) do { \
		if (clk == RCC_GPIOA) LL_IOP_GRP1_DisableClock(LL_IOP_GRP1_PERIPH_GPIOA); \
		if (clk == RCC_USART1) LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_USART1); \
	} while (0)
	#define usart_set_databits(instance, bits) LL_USART_SetDataWidth(instance, bits == 9 ? LL_USART_DATAWIDTH_9B : (bits == 8 ? LL_USART_DATAWIDTH_8B : LL_USART_DATAWIDTH_7B))
	#define usart_recv LL_USART_ReceiveData8
	#define usart_send LL_USART_TransmitData8
	#define USART_ISR_RXNE USART_ISR_RXNE_RXFNE
	#define USART_ISR_TXE USART_ISR_TXE_TXFNF
	#define USART_CR1_TXEIE USART_CR1_TXEIE_TXFNFIE
	#define USART_CR1_RXNEIE USART_CR1_RXNEIE_RXFNEIE
	#define RST_USART1 0
	#define rcc_periph_reset_pulse(instance) LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_USART1)
	#define usart1_isr USART1_IRQHandler
#endif // defined(USE_LL_HAL)

static uint8_t initAddress = 0;
static uint8_t initMask = 0;
static uint8_t configuredAddress = 0;

static const uint32_t BAUD_RATE = 1000000;
static const uint32_t MAX_INTER_FRAME = 150; // us
static const uint32_t INTER_FRAME_BITS = (MAX_INTER_FRAME * BAUD_RATE + 1e6 - 1) / 1e6;

void BusInit(uint8_t initialAddress, uint8_t initialBits) {
	initAddress = initialAddress;
	// Create a mask with initialBits ones (address bits to match)
	// followed by zeroes (address bits to ignore).
	initMask = ~(0x7f >> initialBits);

	BusResetDeviceAddress();

	/* Setup clocks & GPIO for USART */
	rcc_periph_clock_enable(RCC_USART1);
	rcc_periph_clock_enable(RCC_GPIOA);
	#if defined(USE_LL_HAL)
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_9, LL_GPIO_MODE_ALTERNATE);
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_10, LL_GPIO_MODE_ALTERNATE);
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_12, LL_GPIO_MODE_ALTERNATE);
	LL_GPIO_SetAFPin_8_15(GPIOA, LL_GPIO_PIN_9, LL_GPIO_AF_1);
	LL_GPIO_SetAFPin_8_15(GPIOA, LL_GPIO_PIN_10, LL_GPIO_AF_1);
	LL_GPIO_SetAFPin_8_15(GPIOA, LL_GPIO_PIN_12, LL_GPIO_AF_1);
	#else
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9 | GPIO10 | GPIO12);
	// RX & TX & RTS/DE
	gpio_set_af(GPIOA, GPIO_AF1, GPIO9 | GPIO10 | GPIO12);
	#endif

	/* Setup USART parameters. */
	usart_set_baudrate(USART1, BAUD_RATE);
	usart_set_databits(USART1, 8+1); // Includes parity bit
	usart_set_parity(USART1, USART_PARITY_EVEN);
	usart_set_mode(USART1, USART_MODE_TX_RX);

	usart_set_rx_timeout_value(USART1, INTER_FRAME_BITS);
	usart_enable_rx_timeout(USART1);

	// Enable Driver Enable on RTS pin
	USART_CR3(USART1) |= USART_CR3_DEM;

	/* Finally enable the USART. */
	usart_enable(USART1);

	#if defined(RS485_USE_INTERRUPTS)
	USART_CR1(USART1) |= USART_CR1_TXEIE | USART_CR1_RXNEIE | USART_CR1_RTOIE;
	#endif
}

void BusDeinit() {
	rcc_periph_reset_pulse(RST_USART1);

	#if defined(USE_LL_HAL)
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_9, LL_GPIO_MODE_ANALOG);
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_10, LL_GPIO_MODE_ANALOG);
	LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_12, LL_GPIO_MODE_ANALOG);
	LL_GPIO_SetAFPin_8_15(GPIOA, LL_GPIO_PIN_9, LL_GPIO_AF_0);
	LL_GPIO_SetAFPin_8_15(GPIOA, LL_GPIO_PIN_10, LL_GPIO_AF_0);
	LL_GPIO_SetAFPin_8_15(GPIOA, LL_GPIO_PIN_12, LL_GPIO_AF_0);
	#else
	gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO9 | GPIO10 | GPIO12);
	gpio_set_af(GPIOA, GPIO_AF0, GPIO9 | GPIO10 | GPIO12);
	#endif

	rcc_periph_clock_disable(RCC_USART1);
	rcc_periph_clock_disable(RCC_GPIOA);
}

void BusSetDeviceAddress(uint8_t address) {
	// Enable single address
	configuredAddress = address;
}

void BusResetDeviceAddress() {
	configuredAddress = 0;
}

// For RS485, MAX_PACKET_LENGTH is defined including the address byte.
// For requests, the address is stored outside of busBuffer, so this
// buffer is 1 byte too long. However, for replies the adress is stored
// inside the buffer, so use the full MAX_PACKET_LENGTH anyway.
static uint8_t busBuffer[MAX_PACKET_LENGTH];
static uint8_t busBufferLen = 0;
static uint8_t busTxPos = 0;
static uint8_t busAddress = 0;
static_assert(MAX_PACKET_LENGTH < (1 << (sizeof(busBufferLen) * 8)), "Code needs changes for bigger packets");

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
		} else if (busBufferLen < sizeof(busBuffer)) {
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
			busBufferLen = BusCallback(busAddress, busBuffer, busBufferLen, sizeof(busBuffer));
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

#if defined(RS485_USE_INTERRUPTS)
extern "C" void usart1_isr() {
	BusUpdate();
}
#endif
