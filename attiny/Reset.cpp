#include <avr/io.h>
#include <avr/wdt.h>

#include "../BaseProtocol.h"

void resetSystem() {
	wdt_enable(WDTO_15MS);
	while(true) /* wait */;
}
