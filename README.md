# Embedded Linux Systems

A collection of laboratory exercises for the **Embedded Linux Systems** course.

This repository focuses on hands-on Embedded Linux development using the Linux kernel, U-Boot, BusyBox, QEMU, kernel modules, device drivers, and embedded file systems.

## Development Environment

| Component | Configuration |
|---|---|
| Host OS | Ubuntu 24.04 LTS (x86_64) |
| Target Architecture | ARMv7-A |
| Target CPU | ARM Cortex-A9 |
| Emulator | QEMU `vexpress-a9` |
| Linux Kernel | **5.15 LTS** |
| Cross Compiler | `arm-linux-gnueabihf-` |
| Bootloader | U-Boot 2022.04 (built in LAB-01) |
| Root Filesystem | BusyBox 1.35.0 + initramfs |

## Repository Structure

```text
embedded-linux-systems/
├── README.md
├── .gitignore
└── labs/
    ├── lab01/   # Kernel configuration and boot
    └── lab02/   # Character device driver, procfs/sysfs, MTD/JFFS2
```

This repository tracks **configurations, source code, and scripts** — not third-party source trees, generated binaries, or device nodes. The full root filesystem and kernel build are generated locally and are not committed.

## Labs

| Lab | Topic | Status |
|---|---|---|
| LAB-01 | Linux Kernel Configuration and Boot | QEMU boot verified |
| LAB-02 | Device Driver and File System Management | Character driver, procfs/sysfs, MTD/JFFS2 verified |

## Learning Path

```text
LAB-01: Linux Kernel Configuration & Boot
                  |
                  v
LAB-02: Character Driver, procfs/sysfs,
        MTD/JFFS2, BusyBox init auto-load
```

## LAB-01 Overview

LAB-01 creates a minimal ARM Linux environment using:

- Linux Kernel **5.15 LTS**
- ARMv7 Cortex-A9 / QEMU `vexpress-a9`
- U-Boot 2022.04
- BusyBox 1.35.0 with initramfs
- `vexpress-v2p-ca9.dtb` Device Tree

See [labs/lab01/README.md](labs/lab01/README.md) for build and boot instructions.

## LAB-02 Overview

LAB-02 extends LAB-01 with:

- Loadable Linux kernel module (`lab2_driver.ko`), major number 240
- Character device and `file_operations` (`open`, `read`, `write`, `release`)
- Kernel/userspace data transfer (`copy_to_user`, `copy_from_user`)
- Mutex protection for shared data
- procfs (`/proc/lab2_info`) and sysfs (`buffer_len`, `open_count`, `last_data`)
- MTD and JFFS2 on simulated NAND flash (`nandsim`)
- BusyBox init integration — module auto-loads at boot via `rcS`

See [labs/lab02/README.md](labs/lab02/README.md) for build, test, and driver details.

## Basic Git Workflow

```bash
git status
git add .
git commit -m "docs: update embedded Linux lab progress"
git push origin main
```

Commit convention:

```text
feat:     new feature or implementation
fix:      bug fix
docs:     documentation update
test:     test update
build:    build script or Makefile update
refactor: code restructuring
chore:    repository maintenance
```

## Notes

- Do not commit full Linux source trees, temporary kernel build products, or unnecessary generated files.
- Preserve important kernel and BusyBox configurations.
- Keep a `README.md` in each completed lab documenting build, run, and test steps.
- Prefer reproducible build instructions over checked-in binaries.

## Academic Integrity

This repository is maintained for personal coursework, learning, and portfolio purposes. Do not copy another student's code or report. Cite external references where appropriate and follow university course policies.

## Author

**Tran Le Tien Dat (SE205121)**
Embedded Linux Systems
