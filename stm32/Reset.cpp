#include <libopencm3/cm3/scb.h>

#include "../BaseProtocol.h"

void resetSystem() {
	scb_reset_system();
}

