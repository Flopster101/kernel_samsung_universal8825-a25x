#!/bin/bash

# Dependencies
sudo apt install lz4 brotli flex bc cpio -y

# Vars
DATE="$(date '+%Y%m%d-%H%M')"
K_VER="TEST-$DATE"

# Check all args
for arg in "$@"
do
    if [ "$arg" == "clean" ]; then
        echo "clean argument passed, cleaning output directory..."
        make clean
        make mrproper
    fi
done

set -e

if [ ! -d drivers ]; then
    echo "Please exec from top-level kernel tree."
    exit 1
fi

if [ "$(uname -m)" != "x86_64" ]; then
  echo "This script requires an x86_64 (64-bit) machine."
  exit 1
fi

export PATH="$(pwd)/kernel_build/bin:$PATH"

# Configs
OUTDIR="$(pwd)/out"
MODULES_OUTDIR="$(pwd)/modules_out"
TMPDIR="$(pwd)/kernel_build/tmp"
WP="/workspace"

# Device
IN_PLATFORM="$(pwd)/kernel_build/vboot_platform"
IN_DLKM="$(pwd)/kernel_build/vboot_dlkm"
IN_DTB="$OUTDIR/arch/arm64/boot/dts/exynos/s5e8825.dtb"
DEFCONFIG="s5e8825-a25xxx-custom_defconfig"

# Toggles
DOTAR="1"
DOZIP="0"

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
export KBUILD_BUILD_USER="Flopster101"
export KBUILD_BUILD_HOST="buildbot"
KDIR="$(readlink -f .)"
export LLVM=1 LLVM_IAS=1
export ARCH=arm64

kfinish() {
    rm -rf "$TMPDIR"
    # DON'T delete output directory, ugh...
    #rm -rf "$OUTDIR"
    rm -rf "$MODULES_OUTDIR"
}

kfinish

# Toolchain
TCDIR="$WP/toolchain/clang/host/linux-x86/clang-r416183b"
export CROSS_COMPILE="$TCDIR/bin/aarch64-linux-gnu-"
export CC="$TCDIR/bin/clang"
MKBOOTIMG="$(pwd)/kernel_build/mkbootimg/mkbootimg.py"
MKDTBOIMG="$(pwd)/kernel_build/dtb/mkdtboimg.py"
export PATH="$WP/toolchain/prebuilts/build-tools/linux-x86:$TCDIR/bin:$PATH"

# Platform vars
export PLATFORM_VERSION=12
export ANDROID_MAJOR_VERSION=s
export TARGET_SOC=s5e8825

# Run build
echo "Build start"
make -j$(nproc --all) O=out $DEFCONFIG
make -j$(nproc --all) O=out dtbs
make -j$(nproc --all) O=out
make -j$(nproc --all) O=out INSTALL_MOD_STRIP="--strip-debug --keep-section=.ARM.attributes" INSTALL_MOD_PATH="$MODULES_OUTDIR" modules_install

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

if ! find "$MODULES_OUTDIR/lib/modules" -mindepth 1 -type d | read; then
    echo "Unknown error!"
    exit 1
fi

missing_modules=""

find "$MODULES_OUTDIR/lib/modules" -type f -name "*.ko" -exec basename {} \; > "$WP/last_modules.load"

for module in $(cat "$IN_DLKM/modules.load"); do
    i=$(find "$MODULES_OUTDIR/lib/modules" -name $module);
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
echo "Building dtb image..."
python "$MKDTBOIMG" create "$OUT_DTBIMAGE" --custom0=0x00000000 --custom1=0xff000000 --version=0 --page_size=2048 "$IN_DTB" || exit 1

echo "Building boot image..."
$MKBOOTIMG --header_version 4 \
    --kernel "$OUT_KERNEL" \
    --output "$OUT_BOOTIMG" \
    --ramdisk "$PREBUILT_RAMDISK" \
    --os_version 12.0.0 \
    --os_patch_level 2024-09 || exit 1
echo "Done!"

echo "Building vendor_boot image..."
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

echo "Done!"

# Build zip
if [ $DOZIP = 1 ]; then
    echo "Building zip..."
    cd "$(pwd)/kernel_build/zip"
    rm -f "$OUT_KERNELZIP"
    brotli --quality=11 -c boot.img > boot.br
    brotli --quality=11 -c vendor_boot.img > vendor_boot.br
    zip -r9 -q "$OUT_KERNELZIP" META-INF boot.br vendor_boot.br
    rm -f boot.br vendor_boot.br
    cd "$KDIR"
    echo "Done! Output: $OUT_KERNELZIP"
fi

# Build tar
if [ $DOTAR = 1 ]; then
    echo "Building tar..."
    cd "$(pwd)/kernel_build"
    rm -f "$OUT_KERNELTAR"
    lz4 -c -12 -B6 --content-size "$OUT_BOOTIMG" > boot.img.lz4
    lz4 -c -12 -B6 --content-size "$OUT_VENDORBOOTIMG" > vendor_boot.img.lz4
    tar -cf "$OUT_KERNELTAR" boot.img.lz4 vendor_boot.img.lz4
    cd "$KDIR"
    rm -f boot.img.lz4 vendor_boot.img.lz4
    echo "Done! Output: $OUT_KERNELTAR"
fi

# Cleanup
echo "Cleaning..."
rm -f "${OUT_VENDORBOOTIMG}" "${OUT_BOOTIMG}"
kfinish
