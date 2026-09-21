#ifndef KSHIM_DEVICE_H
#define KSHIM_DEVICE_H
#ifdef __cplusplus
extern "C" {
#endif
struct device {
	const char *name;
};
void kshim_log(const struct device *dev, const char *level, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));
#ifdef __cplusplus
}
#endif
#define dev_err(dev, fmt, ...) kshim_log(dev, "err", fmt, ##__VA_ARGS__)
#define dev_warn(dev, fmt, ...) kshim_log(dev, "warn", fmt, ##__VA_ARGS__)
#define dev_info(dev, fmt, ...) kshim_log(dev, "info", fmt, ##__VA_ARGS__)
#define dev_dbg(dev, fmt, ...)                                   \
	do {                                                     \
		if (0)                                           \
			kshim_log(dev, "dbg", fmt, ##__VA_ARGS__); \
	} while (0)
#endif
