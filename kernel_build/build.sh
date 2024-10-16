#!/bin/bash

# Dependencies
UB_DEPLIST="lz4 brotli flex bc cpio kmod ccache zip"
if grep -q "Ubuntu" /etc/os-release; then
    sudo apt install $UB_DEPLIST -y
else
    echo -e "\nINFO: Your distro is not Ubuntu, skipping dependencies installation..."
    echo -e "INFO: Make sure you have these dependencies installed before proceeding: $UB_DEPLIST"
fi

# Vars
DATE="$(date '+%Y%m%d-%H%M')"
K_VER="TEST-$DATE"
if [ -d /workspace ]; then
    WP="/workspace"
    IS_GP=1
else
    IS_GP=0
fi
if [ -z "$WP" ]; then
    echo -e "\nERROR: You haven't set the WP env variable! Please define a workspace...\n"
    exit 1
fi
USE_CCACHE=1

# Check all args
DO_MENUCONFIG=0
for arg in "$@"
do
    if [[ "$arg" == *m* ]]; then
        echo -e "\nINFO: menuconfig argument passed, kernel configuration menu will be shown..."
        DO_MENUCONFIG=1
    fi
    if [[ "$arg" == *c* ]]; then
        echo -e "\nINFO: clean argument passed, cleaning output directory..."
        make clean
        make mrproper
    fi
done

set -e

if [ ! -d drivers ]; then
    echo -e "\nERROR: Please exec from top-level kernel tree\n"
    exit 1
fi

if [ "$(uname -m)" != "x86_64" ]; then
  echo -e "\nERRORThis script requires an x86_64 (64-bit) machine\n"
  exit 1
fi

export PATH="$(pwd)/kernel_build/bin:$PATH"

# Configs
OUTDIR="$(pwd)/out"
MOD_OUTDIR="$(pwd)/modules_out"
TMPDIR="$(pwd)/kernel_build/tmp"

# Device
IN_PLATFORM="$(pwd)/kernel_build/vboot_platform"
IN_DLKM="$(pwd)/kernel_build/vboot_dlkm"
IN_DTB="$OUTDIR/arch/arm64/boot/dts/exynos/s5e8825.dtb"
DEFCONFIG="s5e8825-a25xdxx_defconfig"

# Toggles
DOTAR="1"
DOZIP="1"

# Working paths
PLATFORM_RAMDISK_DIR="$TMPDIR/ramdisk_platform"
DLKM_RAMDISK_DIR="$TMPDIR/ramdisk_dlkm"
PREBUILT_RAMDISK="$(pwd)/kernel_build/boot/ramdisk"
MODULES_DIR="$DLKM_RAMDISK_DIR/lib/modules"

OUT_KERNELZIP="$(pwd)/kernel_build/FlopKernel-${K_VER}_a25x.zip"
OUT_KERNELTAR="$(pwd)/kernel_build/FlopKernel-${K_VER}_a25x.tar"
OUT_KERNEL="$OUTDIR/arch/arm64/boot/Image"
OUT_BOOTIMG="$(pwd)/kernel_build/zip/boot.img"
OUT_VENDORBOOTIMG="$(pwd)/kernel_build/zip/vendor_boot.img"
OUT_DTBIMAGE="$TMPDIR/dtb.img"

# Kernel-side
if [ "$IS_GP" = "1" ]; then
    export KBUILD_BUILD_USER="Flopster101"
    export KBUILD_BUILD_HOST="buildbot"
fi
KDIR="$(readlink -f .)"
export LLVM=1 LLVM_IAS=1
export ARCH=arm64

kfinish() {
    rm -rf "$TMPDIR"
    # DON'T delete output directory, ugh...
    #rm -rf "$OUTDIR"
    rm -rf "$MOD_OUTDIR"
}

kfinish

## Prepare ccache
if [ "$USE_CCACHE" = "1" ]; then
    echo -e "\nINFO: Using ccache\n"
    if [ "$IS_GP" = "1" ]; then
        export CCACHE_DIR=$WP/.ccache
        ccache -M 10G
    else
        echo -e "INFO: Environment is not Gitpod, please make sure you setup your own ccache configuration!\n"
    fi
fi

# Toolchain
TCDIR="$WP/aospclang"
if [ ! -d $TCDIR ]; then
    echo -e "\nINFO: Toolchain not found! Aborting..."
    exit 1
fi
GCC64_DIR="$WP/gcc64"
MKBOOTIMG="$(pwd)/kernel_build/mkbootimg/mkbootimg.py"
MKDTBOIMG="$(pwd)/kernel_build/dtb/mkdtboimg.py"
export PATH="$GCC64_DIR/bin:$TCDIR/bin:$PATH"

# Platform vars
export PLATFORM_VERSION="12"
export ANDROID_PLATFORM_VERSION="12"
export ANDROID_MAJOR_VERSION="s"
export TARGET_SOC="s5e8825"

# Run build
echo -e "INFO: Build start\n"
make -j$(nproc --all) O=out CC="clang" CROSS_COMPILE="aarch64-linux-gnu-" $DEFCONFIG
make -j$(nproc --all) O=out CC="clang" CROSS_COMPILE="aarch64-linux-gnu-" dtbs
if [ $DO_MENUCONFIG = "1" ]; then
    make -j$(nproc --all) O=out CC="clang" CROSS_COMPILE="aarch64-linux-gnu-" menuconfig
fi
if [ $USE_CCACHE = "1" ]; then
    make -j$(nproc --all) O=out CC="ccache clang" CROSS_COMPILE="aarch64-linux-gnu-"
else
    make -j$(nproc --all) O=out CC="clang" CROSS_COMPILE="aarch64-linux-gnu-"
fi
make -j$(nproc --all) O=out CC="clang" CROSS_COMPILE="aarch64-linux-gnu-" INSTALL_MOD_STRIP="--strip-debug --keep-section=.ARM.attributes" INSTALL_MOD_PATH="$MOD_OUTDIR" modules_install

# Post build
rm -rf "$TMPDIR"
rm -f "$OUT_BOOTIMG"
rm -f "$OUT_VENDORBOOTIMG"
mkdir "$TMPDIR"
mkdir -p "$MODULES_DIR/0.0"
mkdir "$PLATFORM_RAMDISK_DIR"

cp -rf "$IN_PLATFORM"/* "$PLATFORM_RAMDISK_DIR/"
mkdir "$PLATFORM_RAMDISK_DIR/first_stage_ramdisk"
cp -f "$PLATFORM_RAMDISK_DIR/fstab.s5e8825" "$PLATFORM_RAMDISK_DIR/first_stage_ramdisk/fstab.s5e8825"

if ! find "$MOD_OUTDIR/lib/modules" -mindepth 1 -type d | read; then
    echo -e "\nERROR: Unknown error!\n"
    exit 1
fi

missing_modules=""

for module in $(cat "$IN_DLKM/modules.load"); do
    i=$(find "$MOD_OUTDIR/lib/modules" -name $module);
    if [ -f "$i" ]; then
        cp -f "$i" "$MODULES_DIR/0.0/$module"
    else
	missing_modules="$missing_modules $module"
    fi
done

if [ "$missing_modules" != "" ]; then
        echo "ERROR: the following modules were not found: $missing_modules"
	exit 1
fi

depmod 0.0 -b "$DLKM_RAMDISK_DIR"
sed -i 's/\([^ ]\+\)/\/lib\/modules\/\1/g' "$MODULES_DIR/0.0/modules.dep"
cd "$MODULES_DIR/0.0"
for i in $(find . -name "modules.*" -type f); do
    if [ $(basename "$i") != "modules.dep" ] && [ $(basename "$i") != "modules.softdep" ] && [ $(basename "$i") != "modules.alias" ]; then
        rm -f "$i"
    fi
done
cd "$KDIR"

cp -f "$IN_DLKM/modules.load" "$MODULES_DIR/0.0/modules.load"
mv "$MODULES_DIR/0.0"/* "$MODULES_DIR/"
rm -rf "$MODULES_DIR/0.0"

# Build the images
echo -e "\nINFO: Building dtb image..."
python "$MKDTBOIMG" create "$OUT_DTBIMAGE" --custom0=0x00000000 --custom1=0xff000000 --version=0 --page_size=2048 "$IN_DTB" || exit 1

echo -e "\nINFO: Building boot image..."
$MKBOOTIMG --header_version 4 \
    --kernel "$OUT_KERNEL" \
    --output "$OUT_BOOTIMG" \
    --ramdisk "$PREBUILT_RAMDISK" \
    --os_version 12.0.0 \
    --os_patch_level 2024-09 || exit 1
echo -e "INFO: Done!"

echo -e "\nINFO: Building vendor_boot image..."
cd "$DLKM_RAMDISK_DIR"
find . | cpio --quiet -o -H newc -R root:root | lz4 -9cl > ../ramdisk_dlkm.lz4
cd ../ramdisk_platform
find . | cpio --quiet -o -H newc -R root:root | lz4 -9cl > ../ramdisk_platform.lz4
cd ..
echo "buildtime_bootconfig=enable" > bootconfig

$MKBOOTIMG --header_version 4 \
    --vendor_boot "$OUT_VENDORBOOTIMG" \
    --vendor_bootconfig "$(pwd)/bootconfig" \
    --dtb "$OUT_DTBIMAGE" \
    --vendor_ramdisk "$(pwd)/ramdisk_platform.lz4" \
    --ramdisk_type dlkm \
    --ramdisk_name dlkm \
    --vendor_ramdisk_fragment "$(pwd)/ramdisk_dlkm.lz4" \
    --os_version 12.0.0 \
    --os_patch_level 2024-09 || exit 1

cd "$KDIR"

echo -e "INFO: Done!"

# Build zip
if [ $DOZIP = 1 ]; then
    echo -e "\nINFO: Building zip..."
    cd "$(pwd)/kernel_build/zip"
    rm -f "$OUT_KERNELZIP"
    brotli --quality=3 -c boot.img > boot.br
    brotli --quality=3 -c vendor_boot.img > vendor_boot.br
    zip -r9 -q "$OUT_KERNELZIP" META-INF boot.br vendor_boot.br
    rm -f boot.br vendor_boot.br
    cd "$KDIR"
    echo -e "INFO: Done! \nINFO: Output: $OUT_KERNELZIP\n"
fi

# Build tar
if [ $DOTAR = 1 ]; then
    echo -e "\nINFO: Building tar..."
    cd "$(pwd)/kernel_build"
    rm -f "$OUT_KERNELTAR"
    lz4 -c -12 -B6 --content-size "$OUT_BOOTIMG" > boot.img.lz4 2>/dev/null
    lz4 -c -12 -B6 --content-size "$OUT_VENDORBOOTIMG" > vendor_boot.img.lz4 2>/dev/null
    tar -cf "$OUT_KERNELTAR" boot.img.lz4 vendor_boot.img.lz4
    rm -f boot.img.lz4 vendor_boot.img.lz4
    echo -e "INFO: Done! \nINFO: Output: $OUT_KERNELTAR\n"
fi

# Cleanup
echo -e "INFO: Cleaning after build..."
rm -f "${OUT_VENDORBOOTIMG}" "${OUT_BOOTIMG}"
kfinish
