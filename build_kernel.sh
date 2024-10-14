#!/bin/bash

export PATH=$(pwd)/toolchain/clang/host/linux-x86/clang-r416183b/bin:$PATH
export PATH=$(pwd)/toolchain/build/build-tools/path/linux-x86:$(pwd)/toolchain/prebuilts/gas/linux-x86:$PATH

make PLATFORM_VERSION=12 ANDROID_MAJOR_VERSION=s LLVM=1 LLVM_IAS=1 ARCH=arm64 TARGET_SOC=s5e8825 CROSS_COMPILE=$(pwd)/toolchain/clang/host/linux-x86/clang-r416183b/bin/aarch64-linux-gnu- s5e8825-a25xxx_defconfig
make PLATFORM_VERSION=12 ANDROID_MAJOR_VERSION=s LLVM=1 LLVM_IAS=1 ARCH=arm64 TARGET_SOC=s5e8825 CROSS_COMPILE=$(pwd)/toolchain/clang/host/linux-x86/clang-r416183b/bin/aarch64-linux-gnu- -j32
