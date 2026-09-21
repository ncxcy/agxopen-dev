#ifndef AGX_POWER_H
#define AGX_POWER_H

#include "agx/types.h"

#define AGX_PWR_OFF 0x000u
#define AGX_PWR_SLEEP 0x001u
#define AGX_PWR_QUIESCED 0x010u
#define AGX_PWR_ON 0x020u
#define AGX_PWR_IDLE 0x201u
#define AGX_PWR_UNKNOWN_202 0x202u
#define AGX_PWR_INIT 0x220u
#define AGX_PWR_LEVEL_MASK 0xffu

enum agx_power_state {
	AGX_POWER_OFF,
	AGX_POWER_QUIESCENT,
	AGX_POWER_STANDBY,
	AGX_POWER_ON
};

AGX_BEGIN_DECLS

const char *agx_power_state_name(enum agx_power_state state);
bool agx_power_state_from_wire(u32 wire, enum agx_power_state *state);

AGX_END_DECLS

#endif
