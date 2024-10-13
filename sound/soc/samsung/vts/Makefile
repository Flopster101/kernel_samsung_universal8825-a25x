snd-soc-samsung-vts-mailbox-$(CONFIG_SND_SOC_SAMSUNG_MAILBOX) += mailbox.o vts_irq.o

ifeq ($(CONFIG_SND_SOC_SAMSUNG_VTS_V1),y)
snd-soc-samsung-vts-$(CONFIG_SND_SOC_SAMSUNG_VTS) += vts_soc_v1.o
else ifeq ($(CONFIG_SND_SOC_SAMSUNG_VTS_V2),y)
snd-soc-samsung-vts-$(CONFIG_SND_SOC_SAMSUNG_VTS) += vts_soc_v2.o
else ifeq ($(CONFIG_SND_SOC_SAMSUNG_VTS_V2_1),y)
snd-soc-samsung-vts-$(CONFIG_SND_SOC_SAMSUNG_VTS) += vts_soc_v2_1.o
else
snd-soc-samsung-vts-$(CONFIG_SND_SOC_SAMSUNG_VTS) += vts_soc.o
endif

snd-soc-samsung-vts-$(CONFIG_SND_SOC_SAMSUNG_VTS) += vts.o \
	vts_qos.o \
	vts_log.o \
	vts_dump.o \
	vts_dma.o \
	vts_util.o \
	vts_res.o \
	vts_pcm_dump.o \
	vts_proc.o
obj-$(CONFIG_SND_SOC_SAMSUNG_MAILBOX) += snd-soc-samsung-vts-mailbox.o
obj-$(CONFIG_SND_SOC_SAMSUNG_VTS) += snd-soc-samsung-vts.o
