CC = i686-elf-gcc
AR = i686-elf-ar
GDB = gdb
NASM = nasm
QEMU = qemu-system-i386.exe
CFLAGS = -ffreestanding -ggdb -Wall -Wextra -Wno-unused-parameter -Iinclude -Iinclude/kstdlib -Iinclude/acpi -D_FUTURA
LDFLAGS = -ffreestanding -ggdb -nostdlib
NASMFLAGS = -felf32 -g

# ------------------------------
# all/clean/run/debug
# ------------------------------

.PHONY: all clean run debug debugscroll debugasm

all: bin/kernel.elf

clean:
	rm -rf bin/kernel.elf
	rm -rf bin/kstdlib.a
	find . -wholename './obj/kernel/*.o' -delete
	find . -wholename './obj/kstdlib/*.o' -delete

run: bin/futura.img
	$(QEMU) \
		-d guest_errors \
		-display gtk,zoom-to-fit=off \
		-vga virtio \
		-drive file=bin/futura.img,if=none,id=DISK1,format=raw \
		-device ahci,id=ahci \
		-device ide-hd,drive=DISK1,bus=ahci.0 \
		-drive file=bin/other.img,if=none,id=DISK2,format=raw \
		-device virtio-blk-pci,drive=DISK2,id=virtblk0 \
		-chardev stdio,id=char0,logfile=bin/serial.log,signal=off \
		-serial chardev:char0

debug: bin/futura.img bin/kernel.elf
	./debug.sh
	$(GDB) bin/kernel.elf \
		-ex "set pagination off" \
		-ex "target remote 10.0.0.106:1234" \
		-ex "set disassembly-flavor intel" \
		-ex "b kmain" \
		-ex "layout src" \
		-ex "continue" \
		-ex "set pagination on"

debugscroll: bin/futura.img bin/kernel.elf
	./debug.sh
	$(GDB) bin/kernel.elf \
		-ex "set pagination off" \
		-ex "target remote 10.0.0.106:1234" \
		-ex "set disassembly-flavor intel" \
		-ex "b TmScroll" \
		-ex "layout src" \
		-ex "continue" \
		-ex "set pagination on"

debugasm: bin/futura.img bin/kernel.elf
	./debug.sh
	$(GDB) bin/kernel.elf \
		-ex "set pagination off" \
		-ex "target remote 10.0.0.106:1234" \
		-ex "set disassembly-flavor intel" \
		-ex "b kinit" \
		-ex "layout asm" \
		-ex "layout reg" \
		-ex "continue" \
		-ex "set pagination on"

# ------------------------------
# futura.img
# ------------------------------

bin/futura.img: scripts/update-img.sh bin/kernel.elf scripts/grub.cfg
	scripts/update-img.sh

# ------------------------------
# kstdlib.a
# ------------------------------

obj/kstdlib/stdio.o: src/kstdlib/stdio.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kstdlib/stdlib.o: src/kstdlib/stdlib.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kstdlib/string.o: src/kstdlib/string.c
	$(CC) -c $^ -o $@ $(CFLAGS)

bin/kstdlib.a: obj/kstdlib/stdio.o obj/kstdlib/stdlib.o obj/kstdlib/string.o
	$(AR) rcs $@ $^

# ------------------------------
# kernel.elf
# ------------------------------

obj/kernel/kstart.o: src/kernel/kstart.nasm
	$(NASM) $(NASMFLAGS) -o $@ $^

obj/kernel/isr.o: src/kernel/isr.nasm
	$(NASM) $(NASMFLAGS) -o $@ $^

obj/kernel/kmain.o: src/kernel/kmain.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/comport.o: src/kernel/comport.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/textmode.o: src/kernel/textmode.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/interrupts.o: src/kernel/interrupts.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/irql.o: src/kernel/irql.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/pic.o: src/kernel/pic.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/apic.o: src/kernel/apic.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/ioapic.o: src/kernel/ioapic.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/pit.o: src/kernel/pit.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/tsc.o: src/kernel/tsc.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/scheduler.o: src/kernel/scheduler.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/bitmap.o: src/kernel/bitmap.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/debug.o: src/kernel/debug.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/heap.o: src/kernel/heap.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/acpiosl.o: src/kernel/acpiosl.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/pci.o: src/kernel/pci.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/drivers/virtio.o: src/kernel/drivers/virtio.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/drivers/virtio_blk.o: src/kernel/drivers/virtio_blk.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/memory.o: src/kernel/memory.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/memory_phys.o: src/kernel/memory_phys.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/memory_virt.o: src/kernel/memory_virt.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/memory_kheap.o: src/kernel/memory_kheap.c
	$(CC) -c $^ -o $@ $(CFLAGS)

obj/kernel/memory_vspace.o: src/kernel/memory_vspace.c
	$(CC) -c $^ -o $@ $(CFLAGS)

bin/kernel.elf: obj/kernel/kstart.o obj/kernel/kmain.o obj/kernel/textmode.o obj/kernel/comport.o obj/kernel/memory.o obj/kernel/memory_phys.o obj/kernel/memory_virt.o obj/kernel/memory_kheap.o obj/kernel/memory_vspace.o obj/kernel/interrupts.o obj/kernel/irql.o obj/kernel/isr.o obj/kernel/pic.o obj/kernel/apic.o obj/kernel/ioapic.o obj/kernel/pit.o obj/kernel/tsc.o obj/kernel/scheduler.o obj/kernel/bitmap.o obj/kernel/debug.o obj/kernel/heap.o obj/kernel/acpiosl.o obj/kernel/pci.o obj/kernel/drivers/virtio.o obj/kernel/drivers/virtio_blk.o bin/kstdlib.a bin/libacpi.a
	$(CC) -T src/kernel/kernel.ld $(LDFLAGS) -o $@ $^ -lgcc
