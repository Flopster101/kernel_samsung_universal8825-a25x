# Samsung sec ABC Feature
obj-$(CONFIG_SEC_ABC)		+= abc.o

# Samsung sec ABC COMMON Feature
abc-$(CONFIG_SEC_ABC_COMMON)	+= abc_common.o

abc-$(CONFIG_SEC_ABC_COMMON)		+= abc_spec_manager.o

abc-$(CONFIG_SEC_ABC_SPEC_TYPE1)		+= abc_spec_manager_type1.o

# Samsung sec ABC MOTTO Feature
abc-$(CONFIG_SEC_ABC_MOTTO)		+= abc_motto.o

# Samsung sec ABC_HUB Feature
obj-$(CONFIG_SEC_ABC_HUB)	+= abc_hub.o

# Samsung sec ABC_HUB Core Feature
abc_hub-$(CONFIG_SEC_ABC_HUB_CORE)	+= abc_hub_core.o

# Samsung sec ABC_HUB Sub Module(Booting time check) Feature
abc_hub-$(CONFIG_SEC_ABC_HUB_BOOTC) += abc_hub_bootc.o

subdir-ccflags-y := -Wformat
