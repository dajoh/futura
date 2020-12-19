#include "comport.h"
#include "lowlevel.h"

#define COM1_IO_PORT            0x3F8
#define COM1_PORT_DATA          (COM1_IO_PORT+0) // When DLAB set to 0. Reading this registers read from the Receive buffer. Writing to this register writes to the Transmit buffer.
#define COM1_PORT_INT_ENABLE    (COM1_IO_PORT+1) // When DLAB set to 0. Interrupt Enable Register.
#define COM1_PORT_DIVISOR_LSB   (COM1_IO_PORT+0) // When DLAB set to 1. Least significant byte of the divisor value for setting the baud rate.
#define COM1_PORT_DIVISOR_MSB   (COM1_IO_PORT+1) // When DLAB set to 1. Most significant byte of the divisor value for setting the baud rate.
#define COM1_PORT_INTID_FIFOCTL (COM1_IO_PORT+2) // Interrupt Identification and FIFO control registers
#define COM1_PORT_LINECTL       (COM1_IO_PORT+3) // Line Control Register. The most significant bit of this register is the DLAB (Divisor Latch Access Bit).
#define COM1_PORT_MODEMCTL      (COM1_IO_PORT+4) // Modem Control Register
#define COM1_PORT_LINE_STATUS   (COM1_IO_PORT+5) // Line Status Register
#define COM1_PORT_MODEM_STATUS  (COM1_IO_PORT+6) // Modem Status Register
#define COM1_PORT_SCRATCH       (COM1_IO_PORT+7) // Scratch Register

void ComInitialize()
{
   outb(COM1_PORT_INT_ENABLE, 0x00);    // Disable all interrupts
   outb(COM1_PORT_LINECTL, 0x80);       // Enable DLAB (set baud rate divisor)
   outb(COM1_PORT_DIVISOR_LSB, 0x03);   // Set divisor to 3 (38400 baud)
   outb(COM1_PORT_DIVISOR_MSB, 0x00);
   outb(COM1_PORT_LINECTL, 0x03);       // 8 bits, no parity, one stop bit
   outb(COM1_PORT_INTID_FIFOCTL, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
   outb(COM1_PORT_MODEMCTL, 0x0B);      // IRQs enabled, RTS/DSR set
}

uint8_t ComHasData()
{
   return inb(COM1_PORT_LINE_STATUS) & 0x01;
}

uint8_t ComRead()
{
    while (!ComHasData())
        ;
    return inb(COM1_PORT_DATA);
}

uint8_t ComCanWrite()
{
   return inb(COM1_PORT_LINE_STATUS) & 0x20;
}

void ComWrite(uint8_t b)
{
    while (!ComCanWrite())
        ;
    outb(COM1_PORT_DATA, b);
}
