#pragma once

#include <stdint.h>
#include <avr/io.h>

struct Pin {
	bool read() const;
	void hiz() const;
	void write(bool value) const;

	volatile uint8_t* port;
	volatile uint8_t* pin;
	volatile uint8_t* ddr;
	volatile uint8_t* pue;
	uint8_t mask;
};

inline bool Pin::read() const {
	*this->ddr &= ~this->mask;
	return (*this->pin) & this->mask;
}

inline void Pin::hiz() const {
	// Hi-Z is just input mode on attiny
	*this->ddr &= ~this->mask;
}

inline void Pin::write(bool value) const {
	if (value)
		*this->port |= this->mask;
	else
		*this->port &= ~this->mask;

	*this->ddr |= this->mask;
}
