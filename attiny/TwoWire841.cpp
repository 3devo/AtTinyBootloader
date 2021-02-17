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

#if defined(__AVR_ATtiny841__) || defined(__AVR_ATtiny441__)

#include "../TwoWire.h"
#include <avr/io.h>
#include <avr/interrupt.h>

static uint8_t initAddress = 0;
static uint8_t initMask = 0;

void TwoWireInit(uint8_t initialAddress, uint8_t initialBits) {
	initAddress = initialAddress;
	// Create a mask with initialBits zeroes (address bits to match)
	// followed by ones (address bits to ignore).
	initMask = (0x7f >> initialBits);

	TwoWireResetDeviceAddress();
	TWSCRB = _BV(TWHNM);

	// Enable Data Interrupt, Address/Stop Interrupt, Two-Wire Interface, Stop Interrpt
	TWSCRA = _BV(TWEN) | _BV(TWSIE);
}

void TwoWireDeinit() {
	TWSCRA = TWSCRB = TWSA = TWSAM = TWSD = 0;
	TWSSRA = _BV(TWDIF) | _BV(TWASIF) | _BV(TWC);
}

void TwoWireSetDeviceAddress(uint8_t address) {
	TWSA = (address << 1) | 1;
	TWSAM = 0;
}

void TwoWireResetDeviceAddress() {
	// Set address and mask to listen for a range of addresses, and
	// enable general call (address 0) recognition by setting TWSA0.
	TWSA = (initAddress << 1) | 1;
	TWSAM = (initMask << 1);
}

static void _Acknowledge(bool ack, bool complete=false) {
	if (ack) {
		TWSCRB &= ~_BV(TWAA);
	} else {
		TWSCRB |= _BV(TWAA);
	}

	TWSCRB |= _BV(TWCMD1) | (complete ? 0 : _BV(TWCMD0));
}


#define TWI_BUFFER_SIZE 32
static uint8_t twiBuffer[TWI_BUFFER_SIZE];
static uint8_t twiBufferLen = 0;
static uint8_t twiReadPos = 0;
static uint8_t twiAddress = 0;

enum TWIState {
	TWIStateIdle,
	TWIStateRead,
	TWIStateWrite
};

static TWIState twiState = TWIStateIdle;



void TwoWireUpdate() {
	uint8_t status = TWSSRA;
	bool dataInterruptFlag = (status & _BV(TWDIF)); // Check whether the data interrupt flag is set
	bool isAddressOrStop = (status & _BV(TWASIF)); // Get the TWI Address/Stop Interrupt Flag
	bool isReadOperation = (status & _BV(TWDIR));
	bool addressReceived = (status & _BV(TWAS)); // Check if we received an address and not a stop

	// Clear the interrupt flags
	// TWSSRA |= _BV(TWDIF) | _BV(TWASIF);

	//volatile bool clockHold = (TWSSRA & _BV(TWCH));
	//volatile bool receiveAck = (TWSSRA & _BV(TWRA));
	//volatile bool collision = (TWSSRA & _BV(TWC));
	//volatile bool busError = (TWSSRA & _BV(TWBE));

	// Handle address received and stop conditions
	if (isAddressOrStop) {
		// If we were previously in a write, then execute the callback and setup for a read.
		if ((twiState == TWIStateWrite) and twiBufferLen != 0) {
			twiBufferLen = TwoWireCallback(twiAddress, twiBuffer, twiBufferLen, TWI_BUFFER_SIZE);
		}

		// Send an ack unless a read is starting and there are no bytes to read.
		bool ack = (twiBufferLen > 0) or (!isReadOperation) or (!addressReceived);
		_Acknowledge(ack, !addressReceived /*complete*/);

		if (!addressReceived) {
			twiState = TWIStateIdle;
		} else if (isReadOperation) {
			twiState = TWIStateRead;
			twiReadPos = 0;
		} else {
			twiState = TWIStateWrite;
			twiBufferLen = 0;
		}

		// The address is in the high 7 bits, the RD/WR bit is in the lsb
		twiAddress = TWSD >> 1;
		return;
	}

	// Data Read
	if (dataInterruptFlag and isReadOperation) {
		if (twiReadPos < twiBufferLen) {
			TWSD = twiBuffer[twiReadPos++];
			_Acknowledge(true /*ack*/, false /*complete*/);
		} else {
			TWSD = 0;
			_Acknowledge(false /*ack*/, true /*complete*/);
		}
		return;
	}

	// Data Write
	if (dataInterruptFlag and !isReadOperation) {
		uint8_t data = TWSD;
		_Acknowledge(true, false);

		if (twiBufferLen < TWI_BUFFER_SIZE) {
			twiBuffer[twiBufferLen++] = data;
		}
		return;
	}
}

#endif
