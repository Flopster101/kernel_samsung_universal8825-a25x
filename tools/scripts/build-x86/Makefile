# This Makefile is a wrapper for invoking Kbuild to build the
# SCSC driver stack as modules on-target.
#
# This Makefile is for building kernel modules for different
# linux version running on HAPS and M.2. Therefore kernel_headers
# for the linux version should be provided as kernel_headers.tar.gz
#
# Kernel headers can be found in scsc-gerrit project:
# /Connectivity/Linux-x86_64-drivers

include .config

# Inject mandatory flags that would usually get generated into
# a header file by Kbuild.
# TODO: This is only a subset of #defines that might need
# TODO: injecting in the long run.
# TODO: Generate all -D flags automagically from .config
#CONFIG_SAMSUNG_MAXWELL_EXTRA += -DCONFIG_SCSC_CORE_FW_LOCATION_AUTO=$(CONFIG_SCSC_CORE_FW_LOCATION_AUTO)
CONFIG_SAMSUNG_MAXWELL_EXTRA += -DCONFIG_SCSC_CORE_FW_LOCATION=$(CONFIG_SCSC_CORE_FW_LOCATION)
CONFIG_SAMSUNG_MAXWELL_EXTRA += -DCONFIG_SCSC_CORE_TOOL_LOCATION_AUTO=$(CONFIG_SCSC_CORE_TOOL_LOCATION_AUTO)

export BUILD_WORKSPACE=scsc_driver/tools/scripts/build-x86/workspace

PWD=$(shell pwd)

# Import <scsc/...>
export
C_INCLUDE_PATH += $(PWD)/scsc_driver/include
# Make environment available to Kbuild

# Invoke Kbuild.
# Build against headers for host Linux (ensure that
# linux-headers-$(uname -r) or an equivalent package is install-
# -ed.
export

KERNEL_BUILD_FILE := ./kernel_headers/build
KERNEL_DIR := kernel_headers
KERNEL_TAR := kernel_headers.tar.gz

MESSAGE := $(KERNEL_DIR) does not exist! Uncompressing !

.PHONY: check_dir

check_dir:
	@[ -d "$(KERNEL_DIR)" ] || { echo "$(MESSAGE)"; \
		mkdir kernel_headers && \
		tar -xzf ${KERNEL_TAR} -C kernel_headers --strip-components=1; }

modules: check_dir
	$(MAKE) -C $(KERNEL_BUILD_FILE) M=${PWD}/${BUILD_WORKSPACE} modules
	mkdir -p ${PWD}/KERNEL_OBJ
	mkdir -p ${PWD}/KERNEL_MODULES
	cp ${PWD}/${BUILD_WORKSPACE}/mx/*.o ${PWD}/KERNEL_OBJ
	cp ${PWD}/${BUILD_WORKSPACE}/wlan/*.o ${PWD}/KERNEL_OBJ
	cp ${PWD}/${BUILD_WORKSPACE}/bt/*.o ${PWD}/KERNEL_OBJ
	cp ${PWD}/${BUILD_WORKSPACE}/mx/*.ko ${PWD}/KERNEL_MODULES
	cp ${PWD}/${BUILD_WORKSPACE}/wlan/*.ko ${PWD}/KERNEL_MODULES
	cp ${PWD}/${BUILD_WORKSPACE}/bt/*.ko ${PWD}/KERNEL_MODULES

# TODO The modules_install build target (builds and) installs modules
# to /lib/modules/$(uname -r)/extra. The directory structure in
# here mirrors that of the Kconfig tree given, so invoke Kbuild
# once for each module.
#
# After installing modules, run depmod to rebuild the module
# dependencies graph. Then modprobe should load modules and their
# dependencies automagically.
# TODO modules_install:
#	$(MAKE) -C $(KERNEL_DIR) M=${PWD}/${BUILD_WORKSPACE} modules_install

clean:
	$(MAKE) -C $(KERNEL_BUILD_FILE) M=${PWD}/${BUILD_WORKSPACE} clean
	rm -rf ${PWD}/KERNEL_OBJ/*
	rm -rf ${PWD}/KERNEL_MODULES/*
	rm ${PWD}/${BUILD_WORKSPACE}/mx/*.mod
	rm ${PWD}/${BUILD_WORKSPACE}/wlan/*.mod
	rm ${PWD}/${BUILD_WORKSPACE}/bt/*.mod

