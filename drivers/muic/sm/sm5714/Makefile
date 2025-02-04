subdir-ccflags-y := -Wformat
obj-$(CONFIG_MUIC_SM5714)	+= muic_sm5714.o
muic_sm5714-y			:= sm5714-muic.o sm5714-muic-afc.o
ifeq ($(CONFIG_MUIC_SUPPORT_PDIC), y)
muic_sm5714-$(CONFIG_MUIC_SM5714)	+= sm5714-muic-pdic.o
else 
	ifeq ($(CONFIG_MUIC_SUPPORT_PDIC), m)
	muic_sm5714-$(CONFIG_MUIC_SM5714)	+= sm5714-muic-pdic.o
	endif
endif
