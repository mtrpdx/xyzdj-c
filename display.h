#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdint.h>

void display_init();
void display_flush_buffer(uint8_t *buf);
void display_cleanup();

#endif // DISPLAY_H_
