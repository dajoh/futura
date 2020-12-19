#!/usr/bin/env bash

sudo umount /dev/loop0p1
sudo partx -d /dev/loop0
sudo losetup -d /dev/loop0
