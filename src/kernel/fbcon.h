#ifndef KERNEL_FBCON_H
#define KERNEL_FBCON_H

#include <stdbool.h>

void ConInitialize(void* fb, int width, int height);
bool ConIsInitialized();

void ConClear(uint8_t color);
void ConPutChar(char chr, uint8_t color);
void ConPutString(const char* str, uint8_t color);

#endif
