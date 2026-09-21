#ifndef KSHIM_DMA_MAPPING_H
#define KSHIM_DMA_MAPPING_H
#include <linux/types.h>
#include <linux/device.h>
typedef unsigned int gfp_t;
#define GFP_KERNEL 0u
#ifdef __cplusplus
extern "C" {
#endif
void *dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *handle,
			 gfp_t gfp);
void dma_free_coherent(struct device *dev, size_t size, void *cpu,
		       dma_addr_t handle);
#ifdef __cplusplus
}
#endif
#endif
