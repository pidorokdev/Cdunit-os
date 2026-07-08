# Dunit OS

Dunit OS is a small multi-architecture hobby operating system with a custom
C/ASM kernel, a RAM-backed desktop, a terminal-first workflow, and the Green
Tea visual theme.

This tree is the desktop branch: it keeps the practical QEMU flow, x86_64 boot
path, RAMFB graphics, PS/2 fallback input, VFS/RamFS, terminal, dock apps, media
viewer, audio hooks, networking experiments, and bundled demo programs.

Rust version of the project: [susopki/dunit-os](https://github.com/susopki/dunit-os).

```text
=== Dunit OS Boot Sequence ===

        ____              _ _
       |  _ \ _   _ _ __ (_) |_
       | | | | | | | '_ \| | __|
       | |_| | |_| | | | | | |_
       |____/ \__,_|_| |_|_|\__|

[ .. ] Starting Dunit OS (Green Tea)
[ OK ] Desktop theme: Green Tea Dark loaded
[ OK ] Dunit shell is running
```

## Build

```bash
./build.sh
```

The x86_64 kernel is written to:

```text
build/x86_64/kernel/dunitos-x86_64.elf
```

The boot image is written to:

```text
image/dunitos-x86_64.img
```

## Run With GUI

```bash
./build.sh gui
```

The GUI target uses QEMU GTK display, RAMFB video, and PS/2 keyboard/mouse
fallbacks. The direct `-kernel` path needs the PVH-capable x86_64 kernel image
from this branch; the make target supplies the right flags.

## Run Headless

```bash
./build.sh run
```

Use headless mode for serial boot logs and quick regression checks.

Advanced make entry points are still available through `Makefile.multiarch`,
but day-to-day Linux usage should go through `./build.sh`.

## Green Tea Identity

- Login prompt: `root@dunit:~#`
- Terminal info command: `dufetch`
- Desktop theme: Green Tea Dark
- Boot palette source: `assets/boot/` and `assets/gui/`
- Dunit artwork: `assets/images/`, `assets/icons/`, `assets/wallpapers/`

## Project Shape

```text
kernel/              kernel, GUI, terminal, drivers, VFS, memory
drivers/             device drivers shared by targets
assets/              Dunit OS artwork and UI resources
build.sh             unified build/run helper
Makefile.multiarch   main multi-architecture build entry
```

## Current Desktop Surface

- Green Tea boot screen and menu bar
- Dock launchers for terminal, files, settings, clock, DOOM, Snake, help, web
- RAMFB framebuffer graphics
- PS/2 keyboard and mouse fallback in QEMU
- Terminal command history, VFS commands, `dufetch`, scripting demos
- RamFS desktop files seeded with Dunit-specific notes

## Notes

This branch intentionally uses Dunit OS naming, artwork, boot text, terminal
prompts, and Green Tea colors throughout the visible system. Legacy naming
should not appear in source, boot logs, build artifacts, or user-facing UI.
