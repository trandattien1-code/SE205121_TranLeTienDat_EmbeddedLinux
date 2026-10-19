# LAB-01 — Boot Notes

## Target and artifacts

| Item | Value |
|---|---|
| Machine | QEMU `vexpress-a9` |
| CPU | ARM Cortex-A9 |
| Kernel | Linux `5.15` LTS |
| Device Tree | `vexpress-v2p-ca9.dtb` |
| Userspace | BusyBox 1.35.0 initramfs |
| Bootloader source | U-Boot 2022.04 |

Generated artifacts are kept locally in `~/embedded_lab1/output/` and are not committed to GitHub:

```text
zImage
vexpress-v2p-ca9.dtb
initramfs.cpio.gz
u-boot
```

## Boot method A — Direct QEMU boot (verified)

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

This loads Linux directly; U-Boot is not invoked in this path.

Inside the QEMU guest:

```sh
uname -r       # expected: 5.15.0
uname -m       # expected: armv7l
hostname      # expected: embedded-lab
mount
```

## Boot method B — Via U-Boot

```text
QEMU -> U-Boot -> zImage + DTB + initramfs -> BusyBox init -> shell
```

In the U-Boot shell:

```text
=> setenv bootargs 'console=ttyAMA0,115200 root=/dev/ram0 rw init=/sbin/init mem=512M'
=> setenv bootcmd 'bootz 0x60100000 0x64000000 0x60000000'
=> saveenv
=> boot
```

Note: vexpress-a9 memory map starts at `0x60000000`, unlike Versatile PB which starts at `0x00000000` — load addresses must be set accordingly.

## Startup files

- `/sbin/init` links to `../bin/busybox`.
- `/etc/inittab` launches `/etc/init.d/rcS`.
- `rcS` mounts `proc`, `sysfs`, and `tmpfs`, and sets the hostname.

See [LAB-01 README](../README.md) for configuration and build details.

## Troubleshooting

- **No console output:** verify `-nographic`, `console=ttyAMA0,115200`, and use the VExpress DTB.
- **No BusyBox shell:** check `/sbin/init`, `/etc/inittab`, executable `rcS`, and `rdinit=/sbin/init`.
- **Kernel panic — VFS: Unable to mount root fs:** check `-initrd` path and `rdinit=`/`root=` in bootargs.
- **Module loading failure in LAB-02:** check `uname -r` inside QEMU and compare it with the module's `vermagic`.
