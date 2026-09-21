#ifndef AGX_RTBUDDY_H
#define AGX_RTBUDDY_H

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include "agx/gpu_ep.h"
#include "agx/power.h"
#include "agx/rtkit.h"
#include "display.h"
#include "mailbox.h"

#define AGX_RTBUDDY_SHMEM_SLOTS 4

struct dentry;

struct agx_shmem {
	void *cpu;
	dma_addr_t iova;
	size_t size;
};

struct agx_rtbuddy {
	struct device *dev;
	void __iomem *base;
	int irq;
	bool irq_requested;
	bool cpu_running;
	bool firmware_up;
	bool doorbell_up;
	struct agx_mailbox mbox;
	struct agx_rtkit rtkit;
	struct agx_display_broker *display;
	enum agx_power_state power;
	u64 drain_count;
	u64 fw_events;
	u64 app_unknown;
	struct dentry *debugfs_root;
	wait_queue_head_t wait;
	struct agx_shmem shmem[AGX_RTBUDDY_SHMEM_SLOTS];
};

int agx_rtbuddy_probe(struct agx_rtbuddy *rb, struct device *dev,
		      void __iomem *base, int irq,
		      struct agx_display_broker *display);
int agx_rtbuddy_boot(struct agx_rtbuddy *rb);
bool agx_rtbuddy_started(struct agx_rtbuddy *rb);
int agx_rtbuddy_send_initdata(struct agx_rtbuddy *rb, u64 initdata_addr);
int agx_rtbuddy_doorbell(struct agx_rtbuddy *rb, u32 channel);
int agx_rtbuddy_kick(struct agx_rtbuddy *rb, u32 index, bool alt);
int agx_rtbuddy_stop_firmware(struct agx_rtbuddy *rb, u64 counter);
int agx_rtbuddy_fwctl_doorbell(struct agx_rtbuddy *rb);
void agx_rtbuddy_remove(struct agx_rtbuddy *rb);

#endif
