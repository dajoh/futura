#!/usr/bin/env bash
qemu-system-i386.exe -s -S \
  -d guest_errors \
  -display gtk,zoom-to-fit=off \
  -vga virtio \
  -drive file=bin/futura.img,if=none,id=DISK1,format=raw \
  -device ahci,id=ahci \
  -device ide-hd,drive=DISK1,bus=ahci.0 \
  -drive file=bin/other.img,if=none,id=DISK2,format=raw \
  -device virtio-blk-pci,drive=DISK2,id=virtblk0 \
  -serial file:bin/serial.log & disown
