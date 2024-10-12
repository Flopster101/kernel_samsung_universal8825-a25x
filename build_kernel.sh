#!/bin/bash
# install flex, bc and device-tree-compiler just in case
sudo apt install flex bc device-tree-compiler -y
# Change to your own, this is only for Gitpod.
WP=/workspace
DEFCONFIG=s5e8825-a25xxx-custom_defconfig
AIKDIR=$WP/AIK-Linux
if [ ! -d drivers ]; then
    printf "\nPlease run inside the top-level kernel tree.\n"
    exit 1
fi
KDIR=$PWD
# Make a tar file?
DOTAR=1

# PATH
export PATH=$WP/toolchain/clang/host/linux-x86/clang-r416183b/bin:$PATH
export PATH=$WP/toolchain/build/build-tools/path/linux-x86:$WP/toolchain/prebuilts/gas/linux-x86:$PATH
#export PATH=/workspace/proton-clang/bin:$PATH

# Platform vars
export PLATFORM_VERSION=12
export ANDROID_MAJOR_VERSION=s
export TARGET_SOC=s5e8825

make LLVM=1 LLVM_IAS=1 ARCH=arm64 O=out CROSS_COMPILE="aarch64-linux-gnu-" $DEFCONFIG
make LLVM=1 LLVM_IAS=1 ARCH=arm64 O=out CROSS_COMPILE="aarch64-linux-gnu-" menuconfig
make LLVM=1 LLVM_IAS=1 ARCH=arm64 O=out CROSS_COMPILE="aarch64-linux-gnu-" savedefconfig
make LLVM=1 LLVM_IAS=1 ARCH=arm64 O=out CROSS_COMPILE="aarch64-linux-gnu-" -j16

if [ ! -f $KDIR/out/arch/arm64/boot/Image ]; then
    printf "\nKernel image not found! Build failed?\n"
    exit 1
fi

if [ $DOTAR = 1 ]; then
    if [ ! -d $AIKDIR ]; then
        git clone https://github.com/Flopster101/AIK-a25x-unpacked $AIKDIR --depth=1
    fi
    printf "\nPacking file for flashing...\n"
    cd $AIKDIR
    cp -f $KDIR/out/arch/arm64/boot/Image split_img/boot.img-kernel
    bash repackimg.sh > /dev/null
    mv image-new.img boot.img
    tar -cvzf boot.tgz boot.img

    curl -T boot.tgz oshi.at
fi

printf "\nDone!\n"