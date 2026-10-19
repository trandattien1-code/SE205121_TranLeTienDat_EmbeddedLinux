# LAB-02 — Linux Character Device Driver

## Overview

This lab builds on LAB-01's minimal Embedded Linux system. It implements a loadable Linux character device driver and exposes device data and statistics through `/dev`, `/proc`, and `/sys`, then uses the MTD subsystem with a simulated NAND flash (`nandsim`) to format, mount, and verify persistence of a JFFS2 filesystem.

## Development Environment

| Component | Configuration |
|---|---|
| Host OS | Ubuntu 24.04 LTS (x86_64) |
| Target CPU / architecture | ARM Cortex-A9 / ARMv7-A |
| Emulator | QEMU `vexpress-a9` |
| Target Linux kernel | 5.15 LTS |
| Cross compiler | `arm-linux-gnueabihf-gcc` |
| Root filesystem | BusyBox + initramfs from LAB-01 |
| Build system | Linux Kbuild (external module) |

The host kernel and target kernel are different. Build the module for **ARMv7 / Linux 5.15** and load it **inside QEMU**, not on the host.

## Repository Structure

```text
labs/lab02/
├── README.md
├── driver/
│   ├── Makefile
│   └── lab2_driver.c
├── scripts/
│   ├── test_driver.sh
│   └── test_procfs.sh
└── rootfs/
    └── initramfs/
        └── etc/
            ├── inittab
            └── init.d/
                └── rcS
```

## Driver Features

- Character device: `/dev/lab2`
- Fixed major/minor: **240:0**
- Kernel buffer capacity: **1024 bytes**
- `open()`, `read()`, `write()`, `release()` via `file_operations`
- Safe userspace transfers using `copy_to_user()` / `copy_from_user()`
- Mutex protection for shared buffer state
- procfs entry: `/proc/lab2_info`
- Read-only sysfs attributes: `buffer_len`, `open_count`, `last_data`

### Data path

```text
Userspace application
    | open / read / write / close
    v
/dev/lab2 (character device, 240:0)
    |
    v
VFS -> file_operations
       | read  -> copy_to_user()   -> user buffer
       | write -> copy_from_user() -> device_buffer
       v
Kernel buffer (1024 bytes)
    |
    +--> /proc/lab2_info              (statistics)
    +--> /sys/class/lab2_class/lab2/  (attributes)
```

## Prerequisites

LAB-01 artifacts must already exist:

```text
~/embedded_lab1/output/zImage
~/embedded_lab1/output/vexpress-v2p-ca9.dtb
~/embedded_lab1/rootfs/initramfs/
~/embedded_lab1/kernel/linux-5.15/
```

Required host tools: `make`, `arm-linux-gnueabihf-gcc`, `qemu-system-arm`, `cpio`, `gzip`, `mtd-utils`, `modinfo`, `file`.

## 1. Build the ARM Kernel Module

```bash
cd labs/lab02/driver
make clean
make
file lab2_driver.ko
modinfo lab2_driver.ko | grep vermagic
```

Expected: `ELF 32-bit LSB relocatable, ARM, EABI5` and `vermagic: 5.15.0 ...`.

Copy the module and test scripts into the LAB-02 rootfs:

```bash
cp lab2_driver.ko ~/embedded_lab2/rootfs/initramfs/lib/modules/5.15.0/
cp ../scripts/test_driver.sh ../scripts/test_procfs.sh ~/embedded_lab2/rootfs/initramfs/usr/bin/
chmod +x ~/embedded_lab2/rootfs/initramfs/usr/bin/test_*.sh
```

## 2. BusyBox init auto-load (Part D)

`rcS` auto-loads the module at boot and creates `/dev/lab2`:

```bash
cp rootfs/initramfs/etc/inittab ~/embedded_lab2/rootfs/initramfs/etc/inittab
cp rootfs/initramfs/etc/init.d/rcS ~/embedded_lab2/rootfs/initramfs/etc/init.d/rcS
chmod +x ~/embedded_lab2/rootfs/initramfs/etc/init.d/rcS
```

Repackage and boot:

```bash
cd ~/embedded_lab2/rootfs/initramfs
find . | cpio -H newc -o | gzip > ~/embedded_lab2/output/initramfs_lab2.cpio.gz

qemu-system-arm \
  -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel ~/embedded_lab1/output/zImage \
  -dtb ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd ~/embedded_lab2/output/initramfs_lab2.cpio.gz \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'
```

Note: since `rcS` already auto-loads the module, running `test_driver.sh` manually afterwards will report `insmod: File exists` on the load step — the module is already resident. This is expected, not a failure.

## 3. MTD / JFFS2 (Part C — runs on the Ubuntu host)

```bash
sudo modprobe nandsim first_id_byte=0x20 second_id_byte=0x35
sudo modprobe mtdblock
sudo modprobe jffs2
cat /proc/mtd

sudo flash_erase /dev/mtd1 0 0

mkdir -p /tmp/jffs2_data/logs
echo 'Hello from JFFS2!' > /tmp/jffs2_data/hello.txt
date > /tmp/jffs2_data/logs/boot.log

sudo mkfs.jffs2 --root=/tmp/jffs2_data --eraseblock=0x4000 \
  --output=/tmp/rootfs.jffs2 --pad --no-cleanmarkers

sudo nandwrite -p /dev/mtd1 /tmp/rootfs.jffs2
sudo mount -t jffs2 /dev/mtdblock1 /mnt/jffs2
cat /mnt/jffs2/hello.txt

# persistence check
sudo umount /mnt/jffs2
sudo mount -t jffs2 /dev/mtdblock1 /mnt/jffs2
cat /mnt/jffs2/hello.txt   # data survives umount/mount

sudo umount /mnt/jffs2
sudo rmmod nandsim
```

## Filesystem comparison

| Filesystem | Suited for | Not suited for |
|---|---|---|
| ext4 | eMMC, SD card, HDD (block abstraction) | Raw NAND/NOR flash |
| JFFS2 | Raw NAND/NOR flash, read/write workload | Large filesystems (slow full-scan mount) |
| cramfs | Read-only firmware image, ROM | Data that needs to change |
| SquashFS | Compressed read-only rootfs | Runtime-writable data |

## Next steps

- Driver architecture diagram and analysis — see lab report.
- Filesystem recommendation for a 32 MB NAND IoT device with OTA updates — see lab report.
