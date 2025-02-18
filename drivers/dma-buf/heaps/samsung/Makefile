# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_DMABUF_HEAPS_SAMSUNG_HPA) += hpa_heap.o
obj-$(CONFIG_DMABUF_CONTAINER)  += dma-buf-container.o
obj-$(CONFIG_DMABUF_SAMSUNG_HEAPS) += samsung_dma_heap.o
samsung_dma_heap-y += heap_dma_buf.o samsung_heap.o dma-buf-trace.o secure_buffer.o
samsung_dma_heap-$(CONFIG_DMABUF_HEAPS_SAMSUNG_CHUNK) += chunk_heap.o
samsung_dma_heap-$(CONFIG_DMABUF_HEAPS_SAMSUNG_CMA) += cma_heap.o
samsung_dma_heap-$(CONFIG_DMABUF_HEAPS_SAMSUNG_CARVEOUT) += carveout_heap.o
samsung_dma_heap-$(CONFIG_DMABUF_HEAPS_SAMSUNG_SYSTEM) += system_heap.o
samsung_dma_heap-$(CONFIG_RBIN) += rbin_heap.o rbincache.o rbinregion.o
