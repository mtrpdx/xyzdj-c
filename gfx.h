#ifndef GFX_H_
#define GFX_H_

#include <stdint.h>
#include <stdbool.h>

/* Screen dimensions SSD1306 128x64 */
#define FB_WIDTH  128
#define FB_HEIGHT 64
#define FB_SIZE   ((FB_WIDTH * FB_HEIGHT) / 8)
// For SH1106, add 2-pixel column offset
// SSD1306 uses column 0, SH1106 uses column 2
/* #define COLUMN_OFFSET 2 */

/* Graphics function prototypes */
void draw_pixel(uint8_t *buf, int x, int y, int color);
void draw_rect(uint8_t *buf, int x, int y, int w, int h, int color);
void draw_filled_rect(uint8_t *buf, int x, int y, int w, int h, int color);
void draw_line(uint8_t *buf, int x0, int y0, int x1, int y1, int color);
void draw_char(uint8_t *buf, int x, int y, char c, bool is_highlighted);
void draw_string(uint8_t *buf, int x, int y, const char *str, bool is_highlighted);

#endif // GFX_H_
