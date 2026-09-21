#ifndef KSHIM_INTERRUPT_H
#define KSHIM_INTERRUPT_H
#ifdef __cplusplus
extern "C" {
#endif
typedef int irqreturn_t;
#define IRQ_NONE 0
#define IRQ_HANDLED 1
#define IRQ_WAKE_THREAD 2
#define IRQF_ONESHOT 0x2000UL
typedef irqreturn_t (*irq_handler_t)(int, void *);
struct device;
int devm_request_threaded_irq(struct device *dev, unsigned int irq,
			      irq_handler_t handler, irq_handler_t thread_fn,
			      unsigned long irqflags, const char *devname,
			      void *dev_id);
void devm_free_irq(struct device *dev, unsigned int irq, void *dev_id);
#ifdef __cplusplus
}
#endif
#endif
