#!/usr/bin/env bash

make
scripts/mount-img.sh
sudo cp scripts/grub.cfg /mnt/FUTURA/boot/grub/
sudo cp bin/kernel.elf /mnt/FUTURA/boot/
sudo sync
scripts/umount-img.sh
