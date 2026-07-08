# Dunit OS Development TODO

This file is the working roadmap for the current C/ASM Dunit OS tree.
Keep it practical: every item should either improve the daily QEMU desktop,
remove a known stub, or make the system easier to debug.

## P0 - Make The Desktop Reliable

- [ ] Verify file manager double-click behavior in QEMU:
  - [x] one click selects files and folders;
  - [ ] double-click opens folders;
  - [ ] double-click opens `.txt` in Notepad;
  - [ ] double-click opens `.bmp`, `.png`, `.jpg`, `.jpeg` in Image Viewer.
- [ ] Add visible error dialogs for unsupported file types instead of silent no-op.
- [ ] Fix remaining input edge cases:
  - [x] keyboard input must not depend on mouse movement;
  - [ ] PS/2 keyboard and mouse must work after window focus changes;
  - [ ] terminal, file manager, image viewer, DOOM, and Snake must not steal input from each other.
- [ ] Add a small GUI debug overlay or serial log toggle for:
  - [ ] focused window;
  - [ ] mouse coordinates and buttons;
  - [ ] last key event;
  - [ ] last launched app/file path.
- [ ] Clean up current `kernel/gui/window.c` warnings introduced by old code.

## P0 - Build And Run Flow

- [ ] Make `./build.sh gui` the canonical command documented everywhere.
- [ ] Add `./build.sh test` for cheap regression checks:
  - [ ] build x86_64 kernel;
  - [ ] build image;
  - [ ] boot QEMU headless long enough to reach GUI init or shell init;
  - [ ] fail if boot logs contain panic or obvious fatal errors.
- [ ] Add dependency check output for Linux packages:
  - [ ] `qemu-system-x86_64`;
  - [ ] `parted`;
  - [ ] `mtools`;
  - [ ] `dosfstools`;
  - [ ] clang/ld/lld or the selected toolchain.
- [ ] Document known-good QEMU versions and flags.
- [x] Keep x86_64 as the default target until arm64 and x86 are genuinely maintained.

## P1 - File Manager

- [ ] Move file opening rules into one small file association table.
- [ ] Support double-click for executable/app launchers instead of extension-only logic.
- [ ] Add path bar editing or clickable breadcrumbs.
- [ ] Implement file copy, paste, delete, and rename with confirmation dialogs.
- [ ] Add directory refresh after create/delete/rename.
- [ ] Add scroll support when a directory has more items than the window can show.
- [ ] Add icon variants for:
  - [ ] image files;
  - [ ] audio files;
  - [ ] scripts;
  - [ ] unknown files;
  - [ ] executable apps.

## P1 - Image Viewer

- [ ] Consolidate duplicated image loading code into one helper:
  - [ ] detect BMP/PNG/JPEG by magic bytes;
  - [ ] return clear decode errors;
  - [ ] free previous image safely.
- [ ] Build folder navigation from real VFS directory entries instead of a hardcoded `/Pictures` list.
- [ ] Preserve zoom mode and rotation per image where reasonable.
- [ ] Add keyboard shortcuts:
  - [ ] left/right for previous/next;
  - [ ] plus/minus for zoom;
  - [ ] `F` for fit;
  - [ ] `Esc` for close/fullscreen exit.
- [ ] Add proper image-file icon preview thumbnails later, after caching is safe.

## P1 - Terminal And Shell

- [ ] Stabilize command editing:
  - [ ] backspace across wrapped lines;
  - [ ] cursor movement;
  - [ ] command history navigation;
  - [ ] paste or simulated paste support from QEMU if possible.
- [ ] Add basic shell commands:
  - [ ] `cat`;
  - [ ] `cp`;
  - [ ] `mv`;
  - [ ] `rm`;
  - [ ] `mkdir`;
  - [ ] `touch`;
  - [ ] `pwd`;
  - [ ] `open <path>`.
- [ ] Make `open <path>` use the same file association logic as the file manager.
- [ ] Add a simple `help` page that matches actual commands.
- [ ] Keep `dufetch` as the identity/status command.

## P1 - VFS And RamFS

- [ ] Audit path handling for root, trailing slash, repeated slash, and long names.
- [ ] Make VFS directory entry names consistently safe for non-null-terminated callbacks.
- [ ] Finish RamFS rename edge cases:
  - [ ] reject overwrite unless explicitly requested;
  - [ ] handle moving across directories;
  - [ ] update parent links correctly.
- [ ] Add simple file metadata:
  - [ ] size;
  - [ ] type;
  - [ ] timestamps when a real clock exists.
- [ ] Add a small VFS test harness that can run during boot or in a debug command.

## P1 - GUI Architecture

- [ ] Split `kernel/gui/window.c` into smaller modules:
  - [ ] compositor/window manager;
  - [ ] file manager;
  - [ ] image viewer;
  - [ ] dock/menu;
  - [ ] settings;
  - [ ] utility drawing helpers.
- [ ] Add a shared widget layer for buttons, lists, toolbar icons, dialogs, and text fields.
- [ ] Stop using global state where per-window state is required.
- [ ] Add modal dialog support for errors and confirmations.
- [ ] Add invalidation/redraw regions later; keep full redraw until correctness is stable.

## P2 - Userspace And Syscalls

- [ ] Decide which apps are kernel-integrated GUI apps and which are userspace programs.
- [ ] Finish basic process lifecycle:
  - [ ] spawn;
  - [ ] exit;
  - [ ] wait;
  - [ ] parent notification;
  - [ ] cleanup.
- [ ] Replace syscall stubs with working minimal implementations:
  - [ ] sleep;
  - [ ] open/read/write/close behavior matching VFS;
  - [ ] exec path that can run a real ELF userspace binary.
- [ ] Build one tiny userspace app end-to-end and launch it from terminal and dock.
- [ ] Keep DOOM isolated from kernel crashes; use sandboxing where possible.

## P2 - Memory And Stability

- [ ] Add allocation failure handling in all GUI/media paths.
- [ ] Add leak checks or debug counters for `kmalloc`/`kfree`.
- [ ] Improve PMM/VMM initialization from real boot memory maps.
- [ ] Add guard checks around large embedded assets and decoded images.
- [ ] Review all fixed-size buffers touched by paths and filenames.
- [ ] Add a panic screen that is readable in GUI and serial output.

## P2 - Drivers And Hardware

- [ ] Keep QEMU x86_64 as the main driver target:
  - [ ] RAMFB graphics;
  - [ ] PS/2 input fallback;
  - [ ] serial logging;
  - [ ] Intel HDA or a simpler audio path.
- [ ] Decide whether VirtIO GPU replaces or complements RAMFB.
- [ ] Finish a reliable audio smoke test before expanding MP3 playback.
- [ ] Track NVMe and ext4 as experimental until read/write paths are verified.
- [ ] Add ACPI discovery tasks after GUI stability.

## P2 - Networking

- [ ] Make current networking experiments explicit in README.
- [ ] Implement a minimal DHCP or static IP config path.
- [ ] Finish ARP cache timestamps using real timer data.
- [ ] Implement UDP send/receive before DNS.
- [ ] Add a tiny network diagnostic command:
  - [ ] `ifconfig`;
  - [ ] `arp`;
  - [ ] `dns`;
  - [ ] later `ping`.

## P3 - Branding And Product Polish

- [ ] Audit for old Vib-OS names in:
  - [ ] source comments;
  - [ ] boot logs;
  - [ ] generated image names;
  - [ ] QEMU output;
  - [ ] user-facing strings.
- [ ] Add a real About Dunit OS window with:
  - [ ] version;
  - [ ] architecture;
  - [ ] build date if available;
  - [ ] Git commit if injected at build time.
- [ ] Add a first-boot desktop note explaining what works today.
- [ ] Keep only Dunit wallpapers and Dunit app icons in visible UI.
- [ ] Add screenshots to README once the desktop stabilizes.

## P3 - Release Hygiene

- [ ] Add `CHANGELOG.md`.
- [ ] Add a simple version file or build define.
- [ ] Add GitHub release artifact workflow later:
  - [ ] build image;
  - [ ] upload `dunitos-x86_64.img`;
  - [ ] upload checksums.
- [ ] Keep generated binary artifacts out of git unless they are intentional embedded assets.
- [ ] Document how to write the image to USB only after real hardware boot is tested.

## Definition Of Done For Near-Term Fixes

- [ ] `./build.sh build` completes.
- [ ] `./build.sh gui` boots to desktop.
- [ ] Terminal accepts keyboard input without mouse movement.
- [ ] File Manager can open folders, text files, and BMP wallpapers by double-click.
- [ ] Image Viewer opens all three bundled Dunit wallpapers.
- [ ] No new compiler warnings are introduced by the change.
- [ ] Commit message describes the user-visible behavior fixed or added.
