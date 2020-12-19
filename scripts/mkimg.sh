#!/usr/bin/env bash

rm bin/futura.img
dd if=/dev/zero of=bin/futura.img bs=1M count=64
fdisk bin/futura.img
sudo partx -a bin/futura.img
sudo mkfs.fat -F 32 -n FUTURA /dev/loop0p1
sudo mount /dev/loop0p1 /mnt/FUTURA
sudo grub-install --root-directory=/mnt/FUTURA --no-floppy --modules="normal part_msdos fat multiboot" /dev/loop0
sudo umount /dev/loop0p1
sudo partx -d /dev/loop0
sudo losetup -d /dev/loop0
