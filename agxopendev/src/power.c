#include "agx/power.h"

const char *agx_power_state_name(enum agx_power_state state)
{
	switch (state) {
	case AGX_POWER_OFF:
		return "off";
	case AGX_POWER_QUIESCENT:
		return "quiescent";
	case AGX_POWER_STANDBY:
		return "standby";
	case AGX_POWER_ON:
		return "on";
	default:
		return "unknown";
	}
}

bool agx_power_state_from_wire(u32 wire, enum agx_power_state *state)
{
	if ((wire & AGX_PWR_LEVEL_MASK) == AGX_PWR_ON) {
		*state = AGX_POWER_ON;
		return true;
	}

	switch (wire) {
	case AGX_PWR_OFF:
		*state = AGX_POWER_OFF;
		return true;
	case AGX_PWR_QUIESCED:
		*state = AGX_POWER_QUIESCENT;
		return true;
	case AGX_PWR_SLEEP:
	case AGX_PWR_IDLE:
		*state = AGX_POWER_STANDBY;
		return true;
	default:
		return false;
	}
}
