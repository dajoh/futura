#!/usr/bin/env bash

sudo partx -a bin/futura.img
sudo mount /dev/loop0p1 /mnt/FUTURA
