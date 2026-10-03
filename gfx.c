#include "mcufont.h"
#include "gfx.h"

#include <stdlib.h>
#include <stdbool.h>

void draw_pixel(uint8_t *buf, int x, int y, int color) {
    /* Set or clear a single pixel */
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;

    int offset = (y * 16) + (x / 8);
    int bit = (x % 8); // LSB-first mapping

    if (color) {
        buf[offset] |= (1 << bit);  // Turn pixel ON
    } else {
        buf[offset] &= ~(1 << bit); // Turn pixel OFF
    }
}

void draw_rect(uint8_t *buf, int x, int y, int w, int h, int color) {
    /* Draw unfilled rectangle */
    for (int i = x; i < x + w; i++) {
        draw_pixel(buf, i, y, color);         // Top edge
        draw_pixel(buf, i, y + h - 1, color); // Bottom edge
    }
    for (int i = y; i < y + h; i++) {
        draw_pixel(buf, x, i, color);         // Left edge
        draw_pixel(buf, x + w - 1, i, color); // Right edge
    }
}

void draw_filled_rect(uint8_t *buf, int x, int y, int w, int h, int color) {
    /* What it says on the tin */
    for (int cur_y = y; cur_y < y + h; cur_y++) {
        for (int cur_x = x; cur_x < x + w; cur_x++) {
            draw_pixel(buf, cur_x, cur_y, color);
        }
    }
}

void draw_line(uint8_t *buf, int x0, int y0, int x1, int y1, int color) {
    /* Draw a line from (x0, y0) to (x1, y1) */
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2; // error value e_xy

    for (;;) {
        draw_pixel(buf, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void draw_char(uint8_t *buf, int x, int y, char c, bool is_highlighted) {
    /* Draw single 5x5 char at x,y */
    const uint8_t* sym = fnt_get_sym(c);
    if (!sym) return;

    for (int row = 0; row < 5; row++) {
        uint8_t row_data = sym[row];
        if (is_highlighted == true) {
            row_data = ~sym[row];
        }
        for (int col = 0; col < 6; col++) {
            // Bit 5 is leftmost/space bit
            uint8_t pixel_on = (row_data << col) & 0b100000;

            int cur_x = x + col;
            int cur_y = y + row;

            if (cur_x < FB_WIDTH && cur_y < FB_HEIGHT) {
                // SSD1306 linear 1bpp mapping:
                // 128 pixels / 8 = 16 bytes per row
                int offset = (cur_y * 16) + (cur_x / 8);
                /* int bit = 7 - (cur_x % 8); // MSB-first */
                int bit = (cur_x % 8); // LSB-first
                if (pixel_on) {
                    buf[offset] |= (1 << bit); // Set bit to 1 (White)
                } else {
                    buf[offset] &= ~(1 << bit); // Set bit to 0 (Black)
                }
            }
        }
    }
}

void draw_string(uint8_t *buf, int x, int y, const char *str, bool is_highlighted) {
    /* Draw string char by char until no more chars */
    while (*str) {
        draw_char(buf, x, y, *str++, is_highlighted);
        x += 6; // Move 6 pixels right for the next char
    }
}
