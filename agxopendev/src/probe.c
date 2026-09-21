#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/version.h>
#include "debugfs.h"
#include "display.h"
#include "rtbuddy.h"

struct agx_device {
	struct agx_rtbuddy rtbuddy;
	struct agx_display_broker display;
};

static const struct of_device_id agx_of_match[] = {
	{ .compatible = "apple,t8132-gfx-asc" },
	{ }
};
MODULE_DEVICE_TABLE(of, agx_of_match);

static int agx_probe(struct platform_device *pdev)
{
	struct agx_device *agx;
	struct resource *res;
	void __iomem *base;
	int irq;
	int ret;

	agx = devm_kzalloc(&pdev->dev, sizeof(*agx), GFP_KERNEL);
	if (!agx)
		return -ENOMEM;

	base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	if (resource_size(res) < AGX_MBOX_REGION_SIZE) {
		dev_err(&pdev->dev,
			"register region is 0x%llx bytes, need at least 0x%x\n",
			(unsigned long long)resource_size(res),
			AGX_MBOX_REGION_SIZE);
		return -EINVAL;
	}

	irq = platform_get_irq_byname(pdev, "recv-not-empty");
	if (irq < 0)
		return irq;

	ret = devm_pm_runtime_enable(&pdev->dev);
	if (ret)
		return ret;

	ret = pm_runtime_resume_and_get(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to power on, error %d\n", ret);
		return ret;
	}

	agx_display_broker_init(&agx->display);

	ret = agx_rtbuddy_probe(&agx->rtbuddy, &pdev->dev, base, irq,
				&agx->display);
	if (ret)
		goto err_power;

	ret = agx_rtbuddy_boot(&agx->rtbuddy);
	if (ret) {
		dev_err(&pdev->dev, "boot sequence failed, error %d\n", ret);
		agx_rtbuddy_remove(&agx->rtbuddy);
		goto err_power;
	}

	agx_debugfs_init(&agx->rtbuddy);
	platform_set_drvdata(pdev, agx);

	dev_info(&pdev->dev, "agx rtbuddy started, ep 0x%02x and 0x%02x up\n",
		 AGX_RTKIT_EP_FIRMWARE, AGX_RTKIT_EP_DOORBELL);

	return 0;

err_power:
	pm_runtime_put_sync(&pdev->dev);
	return ret;
}

static void agx_teardown(struct platform_device *pdev)
{
	struct agx_device *agx = platform_get_drvdata(pdev);

	agx_debugfs_remove(&agx->rtbuddy);
	agx_rtbuddy_remove(&agx->rtbuddy);
	pm_runtime_put_sync(&pdev->dev);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void agx_remove(struct platform_device *pdev)
{
	agx_teardown(pdev);
}
#else
static int agx_remove(struct platform_device *pdev)
{
	agx_teardown(pdev);
	return 0;
}
#endif

static struct platform_driver agx_driver = {
	.probe = agx_probe,
	.remove = agx_remove,
	.driver = {
		.name = "agxrtbuddy",
		.of_match_table = agx_of_match,
	},
};

module_platform_driver(agx_driver);

MODULE_AUTHOR("apex");
MODULE_DESCRIPTION("apple m4 agx gpu rtbuddy client driver");
MODULE_LICENSE("GPL");
