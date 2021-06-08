#pragma once

#include <stdint.h>
#include <avr/io.h>

struct Pin {
	volatile uint8_t* port;
	volatile uint8_t* ddr;
	volatile uint8_t* pue;
	uint8_t mask;
};
