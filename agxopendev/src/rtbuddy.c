#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/sizes.h>
#include <linux/string.h>
#include "rtbuddy.h"

#define AGX_CPU_CTRL_OFFSET 0x44
#define AGX_CPU_CTRL_RUN BIT(4)

#define AGX_RTBUDDY_STAGE_TIMEOUT_MS 500
#define AGX_RTBUDDY_DRAIN_MAX 64
#define AGX_RTBUDDY_SHMEM_MAX SZ_4M

static int agx_rtbuddy_shmem_slot(u32 endpoint)
{
	switch (endpoint) {
	case AGX_RTKIT_EP_CRASHLOG:
		return 0;
	case AGX_RTKIT_EP_SYSLOG:
		return 1;
	case AGX_RTKIT_EP_IOREPORT:
		return 2;
	case AGX_RTKIT_EP_OSLOG:
		return 3;
	default:
		return -1;
	}
}

static void agx_rtbuddy_free_shmem(struct agx_rtbuddy *rb)
{
	unsigned int i;

	for (i = 0; i < AGX_RTBUDDY_SHMEM_SLOTS; i++) {
		struct agx_shmem *shmem = &rb->shmem[i];

		if (!shmem->cpu)
			continue;
		dma_free_coherent(rb->dev, shmem->size, shmem->cpu, shmem->iova);
		shmem->cpu = NULL;
		shmem->iova = 0;
		shmem->size = 0;
	}
}

static void agx_rtbuddy_set_power(struct agx_rtbuddy *rb,
				  enum agx_power_state state)
{
	enum agx_power_state old = rb->power;

	if (old == state)
		return;

	rb->power = state;
	if (rb->display)
		agx_display_notify(rb->display, old, state);
}

static void agx_rtbuddy_stop_cpu(struct agx_rtbuddy *rb)
{
	u32 ctrl;

	if (!rb->cpu_running)
		return;

	ctrl = readl(rb->base + AGX_CPU_CTRL_OFFSET);
	writel(ctrl & ~AGX_CPU_CTRL_RUN, rb->base + AGX_CPU_CTRL_OFFSET);
	rb->cpu_running = false;
}

static int agx_rtbuddy_ops_send(void *ctx, u32 endpoint, u64 message)
{
	struct agx_rtbuddy *rb = ctx;

	return agx_mailbox_send(&rb->mbox, endpoint, message);
}

static int agx_rtbuddy_ops_buffer(void *ctx, u32 endpoint, u64 size,
				  u64 requested_iova, u64 *iova)
{
	struct agx_rtbuddy *rb = ctx;
	struct agx_shmem *shmem;
	int slot = agx_rtbuddy_shmem_slot(endpoint);

	if (slot < 0)
		return -EINVAL;

	if (requested_iova) {
		dev_err(rb->dev,
			"ep 0x%02x asked for a fixed buffer at 0x%llx, not supported\n",
			endpoint, (unsigned long long)requested_iova);
		return -EINVAL;
	}

	if (size > AGX_RTBUDDY_SHMEM_MAX)
		return -EINVAL;

	shmem = &rb->shmem[slot];
	if (shmem->cpu)
		return -EEXIST;

	shmem->cpu = dma_alloc_coherent(rb->dev, size, &shmem->iova, GFP_KERNEL);
	if (!shmem->cpu)
		return -ENOMEM;

	shmem->size = size;
	*iova = shmem->iova;

	dev_dbg(rb->dev, "ep 0x%02x buffer 0x%llx bytes at iova 0x%llx\n",
		endpoint, (unsigned long long)size,
		(unsigned long long)shmem->iova);

	return 0;
}

static void agx_rtbuddy_ops_app(void *ctx, u32 endpoint, u64 message)
{
	struct agx_rtbuddy *rb = ctx;

	if (endpoint == AGX_RTKIT_EP_FIRMWARE && agx_gpu_msg_is_event(message)) {
		rb->fw_events++;
		return;
	}

	rb->app_unknown++;
	dev_dbg(rb->dev, "ep 0x%02x msg 0x%016llx\n", endpoint,
		(unsigned long long)message);
}

static void agx_rtbuddy_ops_crashed(void *ctx)
{
	struct agx_rtbuddy *rb = ctx;

	dev_err(rb->dev, "coprocessor reported a crash\n");
}

static const struct agx_rtkit_ops agx_rtbuddy_rtkit_ops = {
	.send = agx_rtbuddy_ops_send,
	.buffer = agx_rtbuddy_ops_buffer,
	.app = agx_rtbuddy_ops_app,
	.crashed = agx_rtbuddy_ops_crashed,
};

static irqreturn_t agx_rtbuddy_irq(int irq, void *data)
{
	struct agx_rtbuddy *rb = data;

	if (agx_mailbox_outbox_empty(&rb->mbox))
		return IRQ_NONE;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t agx_rtbuddy_irq_thread(int irq, void *data)
{
	struct agx_rtbuddy *rb = data;
	struct agx_mbox_message msg;
	unsigned int handled = 0;

	while (handled < AGX_RTBUDDY_DRAIN_MAX &&
	       !agx_mailbox_recv(&rb->mbox, &msg)) {
		agx_rtkit_rx(&rb->rtkit, msg.endpoint, msg.data);
		handled++;
	}

	rb->drain_count += handled;
	wake_up(&rb->wait);

	return IRQ_HANDLED;
}

static int agx_rtbuddy_wait(struct agx_rtbuddy *rb,
			    bool (*done)(const struct agx_rtkit *rtk))
{
	long left;
	int error;

	left = wait_event_timeout(rb->wait,
				  done(&rb->rtkit) ||
					  agx_rtkit_error(&rb->rtkit) != 0,
				  msecs_to_jiffies(AGX_RTBUDDY_STAGE_TIMEOUT_MS));

	error = agx_rtkit_error(&rb->rtkit);
	if (error)
		return error;
	if (!left)
		return -ETIMEDOUT;

	return 0;
}

int agx_rtbuddy_probe(struct agx_rtbuddy *rb, struct device *dev,
		      void __iomem *base, int irq,
		      struct agx_display_broker *display)
{
	rb->dev = dev;
	rb->base = base;
	rb->irq = irq;
	rb->irq_requested = false;
	rb->cpu_running = false;
	rb->firmware_up = false;
	rb->doorbell_up = false;
	rb->display = display;
	rb->power = AGX_POWER_OFF;
	rb->drain_count = 0;
	rb->fw_events = 0;
	rb->app_unknown = 0;
	rb->debugfs_root = NULL;
	memset(rb->shmem, 0, sizeof(rb->shmem));

	init_waitqueue_head(&rb->wait);
	agx_mailbox_init(&rb->mbox, base);
	agx_rtkit_init(&rb->rtkit, &agx_rtbuddy_rtkit_ops, rb);

	return 0;
}

bool agx_rtbuddy_started(struct agx_rtbuddy *rb)
{
	return agx_rtkit_running(&rb->rtkit) && rb->firmware_up &&
	       rb->doorbell_up;
}

static int agx_rtbuddy_send_gpu(struct agx_rtbuddy *rb, u32 endpoint,
				u64 message)
{
	if (!agx_rtbuddy_started(rb))
		return -ENODEV;

	return agx_mailbox_send(&rb->mbox, endpoint, message);
}

int agx_rtbuddy_send_initdata(struct agx_rtbuddy *rb, u64 initdata_addr)
{
	return agx_rtbuddy_send_gpu(rb, AGX_RTKIT_EP_FIRMWARE,
				    agx_gpu_msg_init(initdata_addr));
}

int agx_rtbuddy_doorbell(struct agx_rtbuddy *rb, u32 channel)
{
	return agx_rtbuddy_send_gpu(rb, AGX_RTKIT_EP_DOORBELL,
				    agx_gpu_msg_doorbell(channel));
}

int agx_rtbuddy_kick(struct agx_rtbuddy *rb, u32 index, bool alt)
{
	return agx_rtbuddy_send_gpu(rb, AGX_RTKIT_EP_DOORBELL,
				    agx_gpu_msg_kick(index, alt));
}

int agx_rtbuddy_stop_firmware(struct agx_rtbuddy *rb, u64 counter)
{
	return agx_rtbuddy_send_gpu(rb, AGX_RTKIT_EP_DOORBELL,
				    agx_gpu_msg_stop(counter));
}

int agx_rtbuddy_fwctl_doorbell(struct agx_rtbuddy *rb)
{
	return agx_rtbuddy_send_gpu(rb, AGX_RTKIT_EP_DOORBELL,
				    agx_gpu_msg_fwctl());
}

int agx_rtbuddy_boot(struct agx_rtbuddy *rb)
{
	u32 ctrl;
	int ret;

	agx_mailbox_set_outbox_enable(&rb->mbox, true);

	ctrl = readl(rb->base + AGX_CPU_CTRL_OFFSET);
	writel(ctrl | AGX_CPU_CTRL_RUN, rb->base + AGX_CPU_CTRL_OFFSET);
	rb->cpu_running = true;

	dev_dbg(rb->dev, "coprocessor cpu started, ctrl=0x%08x\n", ctrl);

	ret = agx_rtkit_set_iop_power(&rb->rtkit, AGX_PWR_INIT);
	if (ret) {
		dev_err(rb->dev, "failed to send iop power INIT, error %d\n", ret);
		goto fail;
	}

	if (!rb->irq_requested) {
		ret = devm_request_threaded_irq(rb->dev, rb->irq,
						agx_rtbuddy_irq,
						agx_rtbuddy_irq_thread,
						IRQF_ONESHOT, "agxrtbuddy", rb);
		if (ret) {
			dev_err(rb->dev, "failed to request irq, error %d\n", ret);
			goto fail;
		}
		rb->irq_requested = true;
	}

	ret = agx_rtbuddy_wait(rb, agx_rtkit_epmap_done);
	if (ret) {
		dev_err(rb->dev, "hello and endpoint map stage failed, error %d\n", ret);
		goto fail;
	}

	ret = agx_rtbuddy_wait(rb, agx_rtkit_iop_acked);
	if (ret) {
		dev_err(rb->dev, "iop power ack stage failed, error %d\n", ret);
		goto fail;
	}

	if ((rb->rtkit.iop_power & AGX_PWR_LEVEL_MASK) != AGX_PWR_ON) {
		dev_err(rb->dev, "iop power state 0x%x after INIT, expected on\n",
			rb->rtkit.iop_power);
		ret = -EIO;
		goto fail;
	}

	ret = agx_rtkit_set_ap_power(&rb->rtkit, AGX_PWR_ON);
	if (ret) {
		dev_err(rb->dev, "failed to send ap power on, error %d\n", ret);
		goto fail;
	}

	ret = agx_rtbuddy_wait(rb, agx_rtkit_ap_acked);
	if (ret) {
		dev_err(rb->dev, "ap power ack stage failed, error %d\n", ret);
		goto fail;
	}

	if (!agx_rtkit_running(&rb->rtkit)) {
		dev_err(rb->dev, "ap power state 0x%x after ack, expected on\n",
			rb->rtkit.ap_power);
		ret = -EIO;
		goto fail;
	}

	ret = agx_rtkit_start_endpoint(&rb->rtkit, AGX_RTKIT_EP_FIRMWARE);
	if (ret) {
		dev_err(rb->dev, "firmware endpoint 0x%02x did not start, error %d\n",
			AGX_RTKIT_EP_FIRMWARE, ret);
		goto fail;
	}
	rb->firmware_up = true;

	ret = agx_rtkit_start_endpoint(&rb->rtkit, AGX_RTKIT_EP_DOORBELL);
	if (ret) {
		dev_err(rb->dev, "doorbell endpoint 0x%02x did not start, error %d\n",
			AGX_RTKIT_EP_DOORBELL, ret);
		goto fail;
	}
	rb->doorbell_up = true;

	agx_rtbuddy_set_power(rb, AGX_POWER_ON);

	dev_info(rb->dev, "coprocessor up, protocol version %u\n",
		 rb->rtkit.version);

	return 0;

fail:
	agx_rtbuddy_stop_cpu(rb);
	rb->firmware_up = false;
	rb->doorbell_up = false;
	return ret;
}

static void agx_rtbuddy_shutdown(struct agx_rtbuddy *rb)
{
	if (!agx_rtkit_running(&rb->rtkit))
		return;

	if (!agx_rtkit_set_ap_power(&rb->rtkit, AGX_PWR_QUIESCED))
		agx_rtbuddy_wait(rb, agx_rtkit_ap_acked);

	if (!agx_rtkit_set_iop_power(&rb->rtkit, AGX_PWR_SLEEP))
		agx_rtbuddy_wait(rb, agx_rtkit_iop_acked);
}

void agx_rtbuddy_remove(struct agx_rtbuddy *rb)
{
	agx_rtbuddy_shutdown(rb);

	if (rb->irq_requested) {
		devm_free_irq(rb->dev, rb->irq, rb);
		rb->irq_requested = false;
	}

	agx_rtbuddy_stop_cpu(rb);
	agx_rtbuddy_free_shmem(rb);

	rb->firmware_up = false;
	rb->doorbell_up = false;
	agx_rtbuddy_set_power(rb, AGX_POWER_OFF);
}
