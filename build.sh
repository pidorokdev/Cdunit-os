#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARCH="${ARCH:-x86_64}"
TARGET="all"

log() {
  printf '[%s] %s\n' "$1" "$2"
}

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing command: $1" >&2
    exit 1
  fi
}

create_x86_64_image() {
  local build_dir="$1"
  local image_dir="$2"
  local image_path="$image_dir/dunitos-x86_64.img"
  local image_size="100M"
  local efi_start_sector=2048
  local efi_offset_bytes=$((efi_start_sector * 512))
  local efi_size_kib=101376
  local kernel="$build_dir/kernel/dunitos-x86_64.elf"

  need_cmd parted
  need_cmd mkfs.vfat
  need_cmd mmd
  need_cmd mcopy

  mkdir -p "$image_dir"
  log UEFI-IMAGE "Creating UEFI disk image: $image_path ($image_size)"
  truncate -s "$image_size" "$image_path"
  parted -s "$image_path" mklabel gpt
  parted -s "$image_path" mkpart ESP fat32 1MiB 100%
  parted -s "$image_path" set 1 esp on
  mkfs.vfat -F 32 -n EFI --offset="$efi_start_sector" "$image_path" "$efi_size_kib" >/dev/null

  mmd -i "$image_path@@$efi_offset_bytes" ::/EFI
  mmd -i "$image_path@@$efi_offset_bytes" ::/EFI/BOOT

  if [[ ! -f "$kernel" ]]; then
    echo "Kernel not found: $kernel" >&2
    exit 1
  fi

  mcopy -i "$image_path@@$efi_offset_bytes" "$kernel" ::/EFI/BOOT/BOOTX64.EFI
  log UEFI-IMAGE "Copied kernel as BOOTX64.EFI"
  log UEFI-IMAGE "Created: $image_path"
  ls -lh "$image_path"
}

create_x86_image() {
  local build_dir="$1"
  local image_dir="$2"
  local image_path="$image_dir/dunitos-x86.img"
  local kernel="$build_dir/kernel/dunitos-x86.elf"

  mkdir -p "$image_dir"
  log BIOS-IMAGE "Creating BIOS disk image: $image_path (100M)"
  dd if=/dev/zero of="$image_path" bs=1M count=100 2>/dev/null

  [[ -f "$build_dir/boot/stage1.bin" ]] &&
    dd if="$build_dir/boot/stage1.bin" of="$image_path" bs=512 count=1 conv=notrunc 2>/dev/null
  [[ -f "$build_dir/boot/stage2.bin" ]] &&
    dd if="$build_dir/boot/stage2.bin" of="$image_path" bs=512 seek=1 conv=notrunc 2>/dev/null
  [[ -f "$kernel" ]] &&
    dd if="$kernel" of="$image_path" bs=512 seek=32 conv=notrunc 2>/dev/null

  log BIOS-IMAGE "Created: $image_path"
  ls -lh "$image_path"
}

create_arm64_image() {
  local build_dir="$1"
  local image_dir="$2"
  local image_path="$image_dir/dunitos-arm64.img"
  local image_size="1G"
  local efi_start_sector=2048
  local efi_offset_bytes=$((efi_start_sector * 512))
  local efi_size_kib=512000
  local kernel="$build_dir/kernel/dunitos-arm64.elf"

  need_cmd parted
  need_cmd mkfs.vfat
  need_cmd mmd
  need_cmd mcopy

  mkdir -p "$image_dir"
  log IMAGE "Creating ARM64 disk image: $image_path ($image_size)"
  truncate -s "$image_size" "$image_path"
  parted -s "$image_path" mklabel gpt \
    mkpart EFI fat32 1MiB 501MiB set 1 esp on \
    mkpart ROOT ext4 501MiB 100%
  mkfs.vfat -F 32 -n EFI --offset="$efi_start_sector" "$image_path" "$efi_size_kib" >/dev/null
  mmd -i "$image_path@@$efi_offset_bytes" ::/EFI
  mmd -i "$image_path@@$efi_offset_bytes" ::/EFI/BOOT

  if [[ -f "$kernel" ]]; then
    mcopy -i "$image_path@@$efi_offset_bytes" "$kernel" ::/EFI/BOOT/kernel.elf
  fi

  log IMAGE "Created: $image_path"
  ls -lh "$image_path"
}

create_image() {
  local arch="$1"
  local build_dir="$2"
  local image_dir="$3"

  case "$arch" in
    x86_64) create_x86_64_image "$build_dir" "$image_dir" ;;
    x86) create_x86_image "$build_dir" "$image_dir" ;;
    arm64) create_arm64_image "$build_dir" "$image_dir" ;;
    *)
      echo "Unsupported arch: $arch" >&2
      exit 2
      ;;
  esac
}

usage() {
  cat <<EOF
Dunit OS build helper

Usage:
  ./build.sh [command] [arch]

Commands:
  build       build kernel and image (default)
  kernel      build kernel only
  image       build boot image
  run         run headless QEMU
  gui         run QEMU with GTK GUI
  debug       run QEMU with GDB server
  clean       clean selected architecture build
  clean-all   clean all build artifacts
  help        show this help

Architectures:
  x86_64      default
  arm64
  x86

Examples:
  ./build.sh
  ./build.sh gui
  ./build.sh run x86_64
  ARCH=arm64 ./build.sh build
EOF
}

case "${1:-build}" in
  image-internal)
    create_image "${2:-$ARCH}" "${3:-$ROOT_DIR/build/${2:-$ARCH}}" "${4:-$ROOT_DIR/image}"
    exit 0
    ;;
  build) TARGET="all" ;;
  kernel|image|clean|clean-all) TARGET="$1" ;;
  help)
    usage
    exit 0
    ;;
  run) TARGET="qemu" ;;
  gui) TARGET="qemu-gui" ;;
  debug) TARGET="qemu-debug" ;;
  x86_64|arm64|x86)
    TARGET="all"
    ARCH="$1"
    ;;
  -h|--help)
    usage
    exit 0
    ;;
  *)
    echo "Unknown command: $1" >&2
    usage >&2
    exit 2
    ;;
esac

if [[ "${2:-}" != "" ]]; then
  ARCH="$2"
fi

case "$ARCH" in
  x86_64|arm64|x86) ;;
  *)
    echo "Unsupported arch: $ARCH" >&2
    echo "Use: x86_64, arm64, or x86" >&2
    exit 2
    ;;
esac

exec make -f "$ROOT_DIR/Makefile.multiarch" -C "$ROOT_DIR" ARCH="$ARCH" "$TARGET"
