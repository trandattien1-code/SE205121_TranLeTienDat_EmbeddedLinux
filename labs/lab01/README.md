# LAB-01 — Embedded Linux Kernel Configuration and Boot

## Overview

LAB-01 builds a minimal ARMv7-A Embedded Linux environment for an emulated Cortex-A9 board using the Linux kernel, Device Tree, BusyBox, initramfs, and QEMU.

## Development environment

| Component | Version / configuration |
|---|---|
| Host OS | Ubuntu 24.04 LTS (x86_64) |
| Target | ARMv7-A, Cortex-A9 |
| QEMU machine | `vexpress-a9` |
| Linux kernel | `5.15` LTS |
| BusyBox | `1.35.0` |
| U-Boot | `2022.04` |
| Cross-compiler | `arm-linux-gnueabihf-gcc` |
| Device Tree | `vexpress-v2p-ca9.dtb` |
| Root filesystem | BusyBox + `initramfs.cpio.gz` |

## Repository layout

```text
labs/lab01/
├── README.md
├── boot/
│   └── README.md
├── configs/
│   ├── kernel.config
│   └── busybox.config
└── rootfs/
    └── initramfs/
        └── etc/
            ├── inittab
            └── init.d/
                └── rcS
```

This repository tracks **configurations and initialization scripts**, not third-party source trees, generated binaries, or device nodes. The full root filesystem is generated locally.

## Local workspace

```text
~/embedded_lab1/
├── kernel/linux-5.15/
├── rootfs/busybox-1.35.0/
├── rootfs/initramfs/
├── uboot/u-boot-2022.04/
└── output/
    ├── zImage
    ├── vexpress-v2p-ca9.dtb
    ├── initramfs.cpio.gz
    └── u-boot
```

## Kernel configuration and build

From the repository root, restore the recorded configuration to the matching Linux 5.15 source tree:

```bash
cp labs/lab01/configs/kernel.config ~/embedded_lab1/kernel/linux-5.15/.config
cd ~/embedded_lab1/kernel/linux-5.15
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- olddefconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j"$(nproc)" zImage dtbs modules
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules_prepare
```

Relevant output paths: `arch/arm/boot/zImage` and `arch/arm/boot/dts/vexpress-v2p-ca9.dtb`.

## BusyBox and initramfs

BusyBox `1.35.0` supplies the minimal userspace. Its configuration is preserved at `configs/busybox.config`.

To restore the init scripts into an existing rootfs:

```bash
cp labs/lab01/rootfs/initramfs/etc/inittab ~/embedded_lab1/rootfs/initramfs/etc/inittab
cp labs/lab01/rootfs/initramfs/etc/init.d/rcS ~/embedded_lab1/rootfs/initramfs/etc/init.d/rcS
chmod +x ~/embedded_lab1/rootfs/initramfs/etc/init.d/rcS
```

Package the rootfs:

```bash
cd ~/embedded_lab1/rootfs/initramfs
find . | cpio -H newc -o | gzip > ~/embedded_lab1/output/initramfs.cpio.gz
```

Device nodes such as `/dev/console` and `/dev/null` require elevated permissions when created. Do not commit those special files to Git.

## Boot with QEMU

```bash
qemu-system-arm \
  -M vexpress-a9 \
  -cpu cortex-a9 \
  -m 512M \
  -smp 2 \
  -nographic \
  -kernel ~/embedded_lab1/output/zImage \
  -dtb ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd ~/embedded_lab1/output/initramfs.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'
```

Expected message:

```text
Boot completed! Welcome to Embedded Linux Lab 1
```

### Verified guest checks

```sh
uname -a
hostname
mount
```

## Boot architecture

```text
QEMU vexpress-a9
    |
    +--> Linux zImage + Device Tree
               |
               v
          initramfs
               |
               v
          BusyBox init
               |
               v
         /etc/inittab
               |
               v
        /etc/init.d/rcS
               |
               v
           /bin/sh
```

See [boot notes](boot/README.md) for U-Boot details and troubleshooting.

## Next lab

[LAB-02](../lab02/README.md) uses the same ARMv7/QEMU environment to build and test a Linux character device driver with `/dev`, procfs, and sysfs interfaces.
