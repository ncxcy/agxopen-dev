#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include "debugfs.h"

static int agx_debugfs_power_show(struct seq_file *seqfile, void *data)
{
	struct agx_rtbuddy *rb = seqfile->private;

	seq_printf(seqfile, "%s\n", agx_power_state_name(rb->power));

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(agx_debugfs_power);

static int agx_debugfs_endpoints_show(struct seq_file *seqfile, void *data)
{
	struct agx_rtbuddy *rb = seqfile->private;

	seq_printf(seqfile, "firmware 0x%02x up %d\n", AGX_RTKIT_EP_FIRMWARE,
		   rb->firmware_up);
	seq_printf(seqfile, "doorbell 0x%02x up %d\n", AGX_RTKIT_EP_DOORBELL,
		   rb->doorbell_up);
	seq_printf(seqfile, "firmware events %llu\n",
		   (unsigned long long)rb->fw_events);
	seq_printf(seqfile, "other app messages %llu\n",
		   (unsigned long long)rb->app_unknown);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(agx_debugfs_endpoints);

static int agx_debugfs_rtkit_show(struct seq_file *seqfile, void *data)
{
	struct agx_rtbuddy *rb = seqfile->private;
	struct agx_rtkit *rtk = &rb->rtkit;

	seq_printf(seqfile, "version %u\n", rtk->version);
	seq_printf(seqfile, "iop power 0x%x\n", rtk->iop_power);
	seq_printf(seqfile, "ap power 0x%x\n", rtk->ap_power);
	seq_printf(seqfile, "running %d\n", agx_rtkit_running(rtk));
	seq_printf(seqfile, "crashed %d\n", agx_rtkit_crashed(rtk));
	seq_printf(seqfile, "error %d\n", agx_rtkit_error(rtk));
	seq_printf(seqfile, "rx %llu\n", (unsigned long long)rtk->rx_count);
	seq_printf(seqfile, "stray %llu\n", (unsigned long long)rtk->stray_count);
	seq_printf(seqfile, "unknown %llu\n", (unsigned long long)rtk->unknown_count);
	seq_printf(seqfile, "last unknown 0x%016llx\n",
		   (unsigned long long)rtk->last_unknown);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(agx_debugfs_rtkit);

void agx_debugfs_init(struct agx_rtbuddy *rb)
{
	rb->debugfs_root = debugfs_create_dir("agxrtbuddy", NULL);

	debugfs_create_file("power", 0444, rb->debugfs_root, rb,
			    &agx_debugfs_power_fops);
	debugfs_create_file("endpoints", 0444, rb->debugfs_root, rb,
			    &agx_debugfs_endpoints_fops);
	debugfs_create_file("rtkit", 0444, rb->debugfs_root, rb,
			    &agx_debugfs_rtkit_fops);
	debugfs_create_u64("draincount", 0444, rb->debugfs_root,
			   &rb->drain_count);
}

void agx_debugfs_remove(struct agx_rtbuddy *rb)
{
	debugfs_remove_recursive(rb->debugfs_root);
	rb->debugfs_root = NULL;
}
