#include "../Config.h"

#if defined(USE_LL_HAL)
#include "stm32g0xx.h"
#else
#include <libopencm3/cm3/scb.h>
#endif

#include "../BaseProtocol.h"

void resetSystem() {
	#if defined(USE_LL_HAL)
		NVIC_SystemReset();
	#else
		scb_reset_system();
	#endif
}

