#include "pic.h"
#include "lowlevel.h"
#include "interrupts.h"

// IO ports
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

// Commands
#define PIC_EOI         0x20  /* End-of-interrupt command code */
#define ICW1_ICW4       0x01  /* ICW4 (not) needed */
#define ICW1_SINGLE     0x02  /* Single (cascade) mode */
#define ICW1_INTERVAL4  0x04  /* Call address interval 4 (8) */
#define ICW1_LEVEL      0x08  /* Level triggered (edge) mode */
#define ICW1_INIT       0x10  /* Initialization - required! */
#define ICW4_8086       0x01  /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO       0x02  /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE  0x08  /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C  /* Buffered mode/master */
#define ICW4_SFNM       0x10  /* Special fully nested (not) */

void PicInitialize()
{
    // Initialize and map the PIC
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4); // starts the initialization sequence (in cascade mode)
    io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC1_DATA, INT20_PIC_IRQ0); // ICW2: Master PIC vector offset
    io_wait();
    outb(PIC2_DATA, INT28_PIC_IRQ8); // ICW2: Slave PIC vector offset
    io_wait();
    outb(PIC1_DATA, 4); // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    io_wait();
    outb(PIC2_DATA, 2); // ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // Mask all IRQs except timer and keyboard
    outb(PIC1_DATA, 0xFC);
    io_wait();
    outb(PIC2_DATA, 0xFF);
    io_wait();
}

void PicDisable()
{
    // Mask all IRQs
    outb(PIC1_DATA, 0xFF);
    io_wait();
    outb(PIC2_DATA, 0xFF);
    io_wait();
}

void PicSendEOI(uint8_t irq)
{
    if(irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}
