TARGET = sec_common_fn

$(TARGET)-objs := sec_input.o sec_cmd.o

ifneq ($(filter y m, $(CONFIG_SEC_DEBUG_TSP_LOG)),)
  $(TARGET)-objs += sec_tsp_log.o
endif
ifneq ($(filter y m, $(CONFIG_INPUT_TOUCHSCREEN_TCLMV2)),)
  $(TARGET)-objs += sec_tclm_v2.o
endif
ifneq ($(filter y m, $(CONFIG_INPUT_SEC_SECURE_TOUCH)),)
  $(TARGET)-objs += sec_secure_touch.o
endif
ifneq ($(filter y m, $(CONFIG_INPUT_SEC_TRUSTED_TOUCH)),)
  $(TARGET)-objs += sec_trusted_touch.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_DUMP_MODE)),)
  $(TARGET)-objs += sec_tsp_dumpkey.o
endif
ifneq ($(filter y m, $(CONFIG_SEC_INPUT_RAWDATA)),)
  $(TARGET)-objs += sec_input_rawdata.o
endif
ifneq ($(filter y m, $(CONFIG_SEC_INPUT_MULTI_DEVICE)),)
  $(TARGET)-objs += sec_input_multi_dev.o
endif

obj-$(CONFIG_INPUT_SEC_INPUT) += $(TARGET).o

# sec_input_notifier need to be separated to avoid dependency cycle error by other modules
obj-$(CONFIG_INPUT_SEC_NOTIFIER) += sec_input_notifier.o

ifeq ($(CONFIG_SEC_KUNIT), y)
endif

ccflags-y += -Wformat

GCOV_PROFILE_sec_input.o := $(CONFIG_SEC_KUNIT)
GCOV_PROFILE_sec_cmd.o := $(CONFIG_SEC_KUNIT)
GCOV_PROFILE_sec_tsp_log.o := $(CONFIG_SEC_KUNIT)
GCOV_PROFILE_sec_tclm_v2.o := $(CONFIG_SEC_KUNIT)
GCOV_PROFILE_sec_tsp_dumpkey.o := $(CONFIG_SEC_KUNIT)

subdir-ccflags-$(CONFIG_SEC_KUNIT) += \
	-Wno-format-zero-length
