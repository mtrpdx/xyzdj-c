// TODO: Animate xyz and add d j text below also animated
// TODO: reorg file structure
// TODO: Change film grain logic to no occlude border

#include "display.h"
#include "gfx.h"
#include "engine3d.h"
#include "audio.h"
#include "aiff.h"
#include "wav.h"

#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <linux/input.h>

#define MAX_FILES_PER_DIR 256

// The maximum number of text lines OLED can fit
#define VISIBLE_LINES 6

#define WAVEFORM_UI_WIDTH 120 // Leave 4 pixels of padding on each side of the OLED
#define WAVEFORM_MAX_HEIGHT 28 // Max pixel height of the waveform lines
#define SAMPLES_PER_PIXEL_BIN 1024 // Adjust this to change waveform zoom level
#define ZOOM_SAMPLES_PER_PIXEL 2048

// --- APPLICATION STATE ---
typedef enum {
    APP_STATE_BROWSER,
    APP_STATE_DECK
} app_state_t;

app_state_t current_app_state = APP_STATE_BROWSER;



deck_state_t active_deck = {0};

/* bool init_alsa_engine(const char* filepath); */
/* void cleanup_alsa_engine(); */

// UI State Variables
int cursor_index = 0;      // Which track is currently highlighted
int window_top_index = 0;  // Which track is drawn at the very top of the OLED
// Marquee Animation State
int last_cursor_index = 0;
int marquee_char_offset = 0;
int marquee_delay_tick = 0;

char current_path[PATH_MAX] = "/mnt/usb";

typedef enum {
    ITEM_FOLDER,
    ITEM_TRACK
} item_type_t;

typedef struct {
    char name[MAX_FILENAME_LEN];
    item_type_t type;
} browser_item_t;

// Master UI data array
browser_item_t current_directory[MAX_FILES_PER_DIR];
int total_items_in_dir = 0;

#define CUBE_SIZE 5
#define NUM_CUBES 3

/* #define RING_BUFFER_SIZE 2048 */

/* typedef struct { */
/*     float data[RING_BUFFER_SIZE]; */
/*     volatile int write_idx; */
/*     volatile int read_idx; */
/* } ring_buffer_t; */

/* ring_buffer_t audio_ring = { .write_idx = 0, .read_idx = 0 }; */

uint8_t bg_fb[FB_SIZE]; // background framebuffer
uint8_t shadow_fb[FB_SIZE]; // shadow framebuffer

// Cube with 8 vertices, centered at (0,0,0)
const vec3_t base_cube[8] = {
    {-1.0f * CUBE_SIZE, -1.0f * CUBE_SIZE, -1.0f * CUBE_SIZE},
    { 1.0f * CUBE_SIZE, -1.0f * CUBE_SIZE, -1.0f * CUBE_SIZE},
    { 1.0f * CUBE_SIZE,  1.0f * CUBE_SIZE, -1.0f * CUBE_SIZE},
    {-1.0f * CUBE_SIZE,  1.0f * CUBE_SIZE, -1.0f * CUBE_SIZE},
    {-1.0f * CUBE_SIZE, -1.0f * CUBE_SIZE,  1.0f * CUBE_SIZE},
    { 1.0f * CUBE_SIZE, -1.0f * CUBE_SIZE,  1.0f * CUBE_SIZE},
    { 1.0f * CUBE_SIZE,  1.0f * CUBE_SIZE,  1.0f * CUBE_SIZE},
    {-1.0f * CUBE_SIZE,  1.0f * CUBE_SIZE,  1.0f * CUBE_SIZE}
};

// 12 lines to connect the vertices
const edge_t cube_edges[12] = {
    // Front
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    // Back
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    // Front to back
    {0, 4}, {1, 5}, {2, 6}, {3, 7}
};


/* void setColumn(uint8_t col) { */
/*   col += COLUMN_OFFSET;  // Shift for SH1106 */

/*   sendCommand(0x00 | (col & 0x0F));        // Low nibble */
/*   sendCommand(0x10 | ((col >> 4) & 0x0F)); // High nibble */
/* } */

#define INPUT_DEVICE "/dev/input/event0"

int input_fd = -1;


void draw_tv_static(uint8_t *buf) {
    for (int i = 0; i < FB_SIZE; i++) {
        // rand() returns a large integer.
        // Bitwise AND (& 0xFF) aggressively chops it down to a single byte.
        buf[i] = rand() & 0xFF;
    }
}

void add_film_grain(uint8_t *buf, int intensity) {
    // intensity: 1 (Very subtle) to 100 (Total chaos)
    for (int i = 0; i < FB_SIZE; i++) {

        // Only inject noise if we hit the random percentage threshold
        if ((rand() % 100) < intensity) {

            // Generate a random byte mask and XOR it with the existing graphics
            uint8_t noise_mask = rand() & 0xFF;
            buf[i] ^= noise_mask;
        }
    }
}

void apply_global_dither(uint8_t *buf) {
    // Iterate through every byte in the framebuffer
    for (int i = 0; i < FB_SIZE; i++) {
        // Determine which physical pixel column this byte belongs to
        int x_col = i % FB_WIDTH;

        // Apply the checkerboard bitmask
        if (x_col % 2 == 0) {
            // Even columns: punch out the odd vertical pixels
            buf[i] &= 0x55;
        } else {
            // Odd columns: punch out the even vertical pixels
            buf[i] &= 0xAA;
        }
    }
}

double get_time_in_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void sleep_ms(int milliseconds) {
    // Convert milliseconds to microseconds
    usleep(milliseconds * 1000);
}

void clear_screen() {
    // Return screen to black
    memset(shadow_fb, 0, FB_SIZE);
    display_flush_buffer(shadow_fb);
}


void debug_splash() {
    // Clear Screen (black)
    memset(shadow_fb, 0, FB_SIZE);
    display_flush_buffer(shadow_fb);
    // startup messages
    draw_string(shadow_fb, 0,  0, "xyzdj ver 0.1", false);
    display_flush_buffer(shadow_fb);
    sleep(1);
    draw_string(shadow_fb, 0, 12, "CHIP: ADSP-SC589", false);
    display_flush_buffer(shadow_fb);
    sleep(1);
    draw_string(shadow_fb, 0, 24, "OLED: SSD1306", false);
    display_flush_buffer(shadow_fb);
    sleep(1);
    printf("Text rendered to framebuffer.\n");

    sleep(5);

    // Return screen to black
    memset(shadow_fb, 0, FB_SIZE);
    display_flush_buffer(shadow_fb);

}


void scan_usb_drive(const char* path) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(path);
    if (dir == NULL) return;

    printf("--- USB Contents ---\n");


    total_items_in_dir = 0; // Reset the counter for the new folder

    while ((entry = readdir(dir)) != NULL) {

        if (total_items_in_dir >= MAX_FILES_PER_DIR) break;

        /* // Skip hidden files and directories (like '.' and '..') */
        /* if (entry->d_name[0] == '.') continue; */

        // skip current directory "."
        if (strcmp(entry->d_name, ".") == 0) continue;
        // skip all hidden files except for parent ".." directory
        if (entry->d_name[0] == '.' && strcmp(entry->d_name, "..") != 0) continue;
        // hide ".." if already at usb root
        if (strcmp(entry->d_name, "..") == 0 && strcmp(path, "/mnt/usb") == 0) continue;

        // Handle Directories
        if (entry->d_type == DT_DIR) {
            strncpy(current_directory[total_items_in_dir].name, entry->d_name, MAX_FILENAME_LEN - 1);
            current_directory[total_items_in_dir].name[MAX_FILENAME_LEN - 1] = '\0'; // Ensure null-termination
            current_directory[total_items_in_dir].type = ITEM_FOLDER;
            total_items_in_dir++;
        }
        else if (entry->d_type == DT_REG && strstr(entry->d_name, ".aiff") != NULL) {
            strncpy(current_directory[total_items_in_dir].name, entry->d_name, MAX_FILENAME_LEN - 1);
            current_directory[total_items_in_dir].name[MAX_FILENAME_LEN - 1] = '\0';
            current_directory[total_items_in_dir].type = ITEM_TRACK;
            total_items_in_dir++;
        }

    }
    closedir(dir);
    // Reset the UI state to the top of the new folder
    cursor_index = 0;
    window_top_index = 0;
}

void draw_browser() {
    clear_screen();
    char display_line[128];
    int lines_to_draw = VISIBLE_LINES;

    // header
    draw_string(bg_fb, 2, 4, "Please select a file", false);
    // header divider line
    draw_line(bg_fb, 0, 12, FB_WIDTH, 12, 1);
    memcpy(shadow_fb, bg_fb, FB_SIZE);

    // --- MARQUEE RESET LOGIC ---
    // If the user moved the cursor, instantly reset the scrolling animation
    if (cursor_index != last_cursor_index) {
        marquee_char_offset = 0;
        marquee_delay_tick = 0;
        last_cursor_index = cursor_index;
    }

    for (int i = 0; i < lines_to_draw; i++) {
        int item_idx = window_top_index + i;

        // Stop drawing if we hit the bottom of the list early
        if (item_idx >= total_items_in_dir) break;

        const char* type_prefix;
        const char* type_suffix;
        if (current_directory[item_idx].type == ITEM_FOLDER) {
            type_prefix = "[DIR] ";
            type_suffix = "/";
        } else {
            type_prefix = "";
            type_suffix = ""; // Blank space for tracks to keep text aligned
        }

        snprintf(display_line, sizeof(display_line), "%s%s",
                 /* type_prefix, */
                 current_directory[item_idx].name,
                 type_suffix);


        bool is_highlighted = (item_idx == cursor_index);
        int y_position = (i * 8) + 16;
        // int y_position = (i * 6) + 16;
        // The pointer to the start of the string we will actually draw
        const char* string_to_draw = display_line;

        // --- MARQUEE SCROLL LOGIC ---
        int max_chars_on_screen = 19; // Adjust based on your font width and scrollbar gap

        if (is_highlighted && strlen(display_line) > max_chars_on_screen) {
            marquee_delay_tick++;

            // Wait roughly 0.5 seconds (30 frames at 60fps) before scrolling
            if (marquee_delay_tick > 30) {
                // Move over by 1 character every 6 frames for a smooth reading speed
                if (marquee_delay_tick % 6 == 0) {
                    marquee_char_offset++;

                    // If we scrolled past the end, snap back to the start
                    if (marquee_char_offset > (strlen(display_line) - max_chars_on_screen + 3)) {
                        marquee_char_offset = 0;
                        marquee_delay_tick = 0; // Trigger the 0.5s pause again
                    }
                }
            }
            // Shift the pointer forward in memory to "scroll" the text
            string_to_draw = display_line + marquee_char_offset;
            /* int length = (strlen(display_line) * 6) + 1; */
            /* draw_filled_rect(shadow_fb, 3, y_position - 1, length, 7, 1); */
        }

        // --- THE FIX: STRICT STRING CLIPPING ---
        // Create a safe, temporary buffer just for the physical screen slice
        char safe_render_string[32];
        // Copy only the allowed amount of characters into the safe buffer
        strncpy(safe_render_string, string_to_draw, max_chars_on_screen);
        // Forcefully lock the string down so draw_string cannot wrap around
        safe_render_string[max_chars_on_screen] = '\0';
        // Draw Highlight Box (Restricted width so it doesn't bleed into the scrollbar)
        if (is_highlighted == true) {
            draw_filled_rect(shadow_fb, 3, y_position - 1, FB_WIDTH - 6, 7, 1);
        }

        // Draw to your OLED buffer
        draw_string(
            shadow_fb,
            4,
            y_position, // i * 10 pixels down
            safe_render_string,
            is_highlighted
        );
    }

    /* // --- SCROLLBAR LOGIC --- */
    /* if (total_items_in_dir > VISIBLE_LINES) { */
    /*     // 1. The Occlusion Mask */
    /*     // Draw a solid black rectangle down the right edge. This forces a clean "gap" */
    /*     // between the text and the scrollbar, erasing any text that bled too far right. */
    /*     draw_filled_rect(shadow_fb, FB_WIDTH - 9, 14, 9, FB_HEIGHT - 14, 0); // 0 = black */

    /*     int track_top = 16; */
    /*     int track_height = FB_HEIGHT - 18; */

    /*     // Calculate proportional handle size */
    /*     int handle_h = (VISIBLE_LINES * track_height) / total_items_in_dir; */
    /*     if (handle_h < 4) handle_h = 4; // Minimum 4px height so it doesn't vanish on huge folders */

    /*     // Calculate Y position based on current scroll percentage */
    /*     float scroll_pct = (float)window_top_index / (total_items_in_dir - VISIBLE_LINES); */
    /*     int handle_y = track_top + (int)(scroll_pct * (track_height - handle_h)); */

    /*     // Draw the vertical track line */
    /*     draw_line(shadow_fb, FB_WIDTH - 3, track_top, FB_WIDTH - 3, track_top + track_height, 1); */

    /*     // Draw the moving handle */
    /*     draw_filled_rect(shadow_fb, FB_WIDTH - 5, handle_y, 5, handle_h, 1); */
    /* } */

    // --- MINIMALIST SCROLLBAR LOGIC ---
    /* if (total_items_in_dir > VISIBLE_LINES) { */

    /*     // 1. The Precision Occlusion Mask */
    /*     // Clear the text bleed, but STOP 1 pixel before the right edge to preserve */
    /*     // the background frame border that was copied from bg_fb. */
    /*     // This clears a 5-pixel wide gap starting 6 pixels from the right edge. */
    /*     draw_filled_rect(shadow_fb, FB_WIDTH - 6, 13, 5, FB_HEIGHT - 14, 0); */

    /*     // Define the track boundaries (starts just below the header divider) */
    /*     int track_top = 13; */
    /*     int track_height = FB_HEIGHT - 14; */

    /*     // Calculate proportional handle size */
    /*     int handle_h = (VISIBLE_LINES * track_height) / total_items_in_dir; */
    /*     if (handle_h < 4) handle_h = 4; // Minimum 4px height so it doesn't vanish */

    /*     // Calculate Y position based on current scroll percentage */
    /*     float scroll_pct = (float)window_top_index / (total_items_in_dir - VISIBLE_LINES); */
    /*     int handle_y = track_top + (int)(scroll_pct * (track_height - handle_h)); */

    /*     // 2. The Flush Handle */
    /*     // Draw the handle starting 4 pixels from the right, with a width of 4. */
    /*     // This makes the rightmost pixel overlap the frame border perfectly, */
    /*     // leaving the left 3 pixels sticking out cleanly into the gap. */
    /*     draw_filled_rect(shadow_fb, FB_WIDTH - 4, handle_y, 4, handle_h, 1); */
    /* } */

    // --- OTHER MINIMALIST SCROLLBAR LOGIC ---
    if (total_items_in_dir > VISIBLE_LINES) {
        // 1. The Safer Occlusion Mask
        // Shifted to start 8 pixels away from the right edge.
        // This clears a 6-pixel wide gap, giving the background frame plenty of breathing room.
        draw_filled_rect(shadow_fb, FB_WIDTH - 6, 13, 4, FB_HEIGHT - 14, 0);

        int track_top = 13;
        int track_height = FB_HEIGHT - 14;
        int handle_h = (VISIBLE_LINES * track_height) / total_items_in_dir;
        if (handle_h < 4) handle_h = 4;

        float scroll_pct = (float)window_top_index / (total_items_in_dir - VISIBLE_LINES);
        int handle_y = track_top + (int)(scroll_pct * (track_height - handle_h));

        // 2. The Floating Handle
        // We draw the handle 5 pixels from the right, with a width of 3.
        // It will now sit perfectly flush against the inner edge of the background frame
        // without physically overlapping it.
        draw_filled_rect(shadow_fb, FB_WIDTH - 4, handle_y, 3, handle_h, 1);
    }
    display_flush_buffer(shadow_fb);
}

void update_browser_scroll(int direction) {

    // Safety check: Don't scroll if the folder is empty
    if (total_items_in_dir == 0) return;

    // --- SCROLLING DOWN (Clockwise) ---
    if (direction > 0) {
        // 1. Can we move the cursor down?
        if (cursor_index < total_items_in_dir - 1) {
            cursor_index++;

            // 2. Did the cursor drop off the bottom of the screen?
            // If so, push the window down by 1 so the cursor is visible again.
            if (cursor_index >= window_top_index + VISIBLE_LINES) {
                window_top_index++;
            }
        }
    }

    // --- SCROLLING UP (Counter-Clockwise) ---
    else if (direction < 0) {
        // 1. Can we move the cursor up?
        if (cursor_index > 0) {
            cursor_index--;

            // 2. Did the cursor push past the top of the screen?
            // If so, pull the window up by 1.
            if (cursor_index < window_top_index) {
                window_top_index--;
            }
        }
    }
}

void draw_deck() {
    clear_screen();

    // 1. Draw the Outer Border
    draw_rect(shadow_fb, 0, 0, FB_WIDTH, FB_HEIGHT, 1);

    // 2. Draw the Track Name (Top Bar)
    // Truncate name to fit the screen (~19 chars max)
    char safe_title[20];
    strncpy(safe_title, active_deck.track_name, 19);
    safe_title[19] = '\0';
    draw_string(shadow_fb, 4, 4, safe_title, false);
    draw_line(shadow_fb, 0, 12, FB_WIDTH, 12, 1); // Header divider

    // 3. Draw Track Info (BPM and State)
    char info_buf[32];
    snprintf(info_buf, sizeof(info_buf), "BPM:%.1f", active_deck.bpm);
    draw_string(shadow_fb, 4, 16, info_buf, false);

    if (active_deck.play_state == DECK_PLAYING) {
        draw_string(shadow_fb, 80, 16, "[PLAY]", true); // Highlighted
    } else {
        draw_string(shadow_fb, 80, 16, "[CUED]", false);
    }

    /* // 4. Draw the Waveform (Center) */
    /* int wave_center_y = 42; */
    /* int wave_start_x = 4; */

    /* for (int i = 0; i < active_deck.num_peaks; i++) { */
    /*     int peak_h = active_deck.visual_peaks[i]; */
    /*     if (peak_h == 0) peak_h = 1; // Always draw at least a center line */

    /*     // Draw a vertical line for this chunk of audio */
    /*     draw_line(shadow_fb, */
    /*               wave_start_x + i, wave_center_y - (peak_h / 2), */
    /*               wave_start_x + i, wave_center_y + (peak_h / 2), */
    /*               1); */
    /* } */

    // 4. Draw the Scrolling Waveform (Center)
    int wave_center_y = 42;
    int wave_start_x = 4;

    // Lock the playhead to the physical center of the screen
    int playhead_screen_x = wave_start_x + (WAVEFORM_UI_WIDTH / 2);

    // Calculate exactly which peak in the massive array the playhead is currently over
    int center_peak_idx = active_deck.current_sample_pos / ZOOM_SAMPLES_PER_PIXEL;

    // Draw the 120-pixel window
    for (int screen_x = 0; screen_x < WAVEFORM_UI_WIDTH; screen_x++) {

        // Map the screen's X pixel to the massive track array
        // Subtract half the screen width to look "backward" into the past,
        // and add screen_x to sweep forward into the future.
        int track_idx = center_peak_idx - (WAVEFORM_UI_WIDTH / 2) + screen_x;

        // Check array boundaries (we don't want to draw garbage memory before the
        // track starts or after it ends)
        if (track_idx >= 0 && track_idx < active_deck.num_peaks) {

            int peak_h = active_deck.visual_peaks[track_idx];
            if (peak_h == 0) peak_h = 1; // Draw a flat center line during complete silence

            draw_line(shadow_fb,
                      wave_start_x + screen_x, wave_center_y - (peak_h / 2),
                      wave_start_x + screen_x, wave_center_y + (peak_h / 2),
                      1);
        } else {
            // Draw a flat line for out-of-bounds (before song starts / after song ends)
            draw_pixel(shadow_fb, wave_start_x + screen_x, wave_center_y, 1);
        }
    }

    // 5. Draw the Playhead Cursor
    // Calculate where the cursor is visually on the 120-pixel grid
    /* int playhead_x = wave_start_x + (int)((float)active_deck.current_sample_pos / active_deck.metadata.total_samples * WAVEFORM_UI_WIDTH); */

    // Draw a full-height line over the waveform to show exact position
    draw_line(shadow_fb, playhead_screen_x, wave_center_y - 15, playhead_screen_x, wave_center_y + 15, 0); // Erase background
    draw_line(shadow_fb, playhead_screen_x - 1, wave_center_y - 15, playhead_screen_x - 1, wave_center_y + 15, 1); // Draw cursor edges
    draw_line(shadow_fb, playhead_screen_x + 1, wave_center_y - 15, playhead_screen_x + 1, wave_center_y + 15, 1);

    display_flush_buffer(shadow_fb);
}

// Toggle playback when the user hits the PLAY button
void toggle_playback(deck_state_t* deck) {
    if (deck->play_state == DECK_CUED) {
        deck->play_state = DECK_PLAYING;
        printf("Playing track at BPM: %.1f\n", deck->bpm);
        // TODO: Tell the ALSA audio driver to start consuming samples
    } else {
        deck->play_state = DECK_CUED;
        printf("Track Cued.\n");
        // TODO: Tell the ALSA audio driver to pause
    }
}

void cubes_animation_loop() {
    /* float angle_x = 0.0f; */
    /* float angle_y = 0.0f; */
    /* float angle_z = 0.0f; */

    instance_t scene[NUM_CUBES] = {
        // Left Cube (X = -25)
        {-25.0f, 0.0f, 0.0f,   0.0f, 0.0f, 0.0f,   2.0f, 3.0f, 0.0f},
        // Center Cube (X = 0)
        {  0.0f, 0.0f, 0.0f,   0.0f, 0.0f, 0.0f,   -1.0f, 1.0f, 2.0f},
        // Right Cube (X = 25)
        { 25.0f, 0.0f, 0.0f,   0.0f, 0.0f, 0.0f,   0.0f, 4.0f, 1.0f}
    };

    // Arrays for the pipeline
    vec3_t rotated_points[8];
    vec2_t projected_points[8];

    // 1. Draw a main border around the whole screen
    draw_rect(bg_fb, 0, 0, FB_WIDTH, FB_HEIGHT, 1);

    draw_char(bg_fb, 12, 30, 'x', false);
    draw_char(bg_fb, 61, 30, 'y', false);
    draw_char(bg_fb, 111, 30, 'z', false);

    draw_char(bg_fb, 48, 50, 'd', false);
    draw_char(bg_fb, 73, 50, 'j', false);

    int frames_spent = 0;
    while (frames_spent < 100) { // Animation Loop ~2seconds
        memset(shadow_fb, 0, FB_SIZE);
        memcpy(shadow_fb, bg_fb, FB_SIZE);

        // Instancing loop
        for (int c = 0; c < NUM_CUBES; c++) {

            // Update one cube rotation
            scene[c].rot_x += scene[c].spin_x;
            scene[c].rot_y += scene[c].spin_y;
            scene[c].rot_z += scene[c].spin_z;

            // Bounds Checking, prevent float overflow for long uptimes
            if (scene[c].rot_x >= 360.0f) scene[c].rot_x -= 360.0f;
            if (scene[c].rot_y >= 360.0f) scene[c].rot_y -= 360.0f;
            if (scene[c].rot_z >= 360.0f) scene[c].rot_z -= 360.0f;

            // Handle negative bounds for cubes with negative spin
            if (scene[c].rot_x < 0.0f) scene[c].rot_x += 360.0f;
            if (scene[c].rot_y < 0.0f) scene[c].rot_y += 360.0f;
            if (scene[c].rot_z < 0.0f) scene[c].rot_z += 360.0f;

            // Transform the 3D points
            for (int i = 0; i < 8; i++) {
                // Grab a fresh copy
                rotated_points[i] = base_cube[i];

                // Rotate the copy
                rotate_3d(&rotated_points[i], scene[c].rot_x, scene[c].rot_y, scene[c].rot_z);

                // Move it to its location in the world (World Space)
                translate_3d(&rotated_points[i], scene[c].pos_x, scene[c].pos_y, scene[c].pos_z);
                // Project the 3D copy into a 2D screen coordinate
                // FOV = 120, Camera Distance = 30
                projected_points[i] = project_3d_to_2d(rotated_points[i], 120, 60);
            }

            // Draw the geometry
            for (int i = 0; i < 12; i++) {
                int v1 = cube_edges[i].v1;
                int v2 = cube_edges[i].v2;

                // Connect the projected dots
                draw_line(shadow_fb, projected_points[v1].x,
                          projected_points[v1].y, projected_points[v2].x,
                          projected_points[v2].y, 1);
            }
        }

        /* // 3. Increment rotation for the next frame */
        /* angle_x += 2.0f; */
        /* angle_y += 3.0f; */
        /* angle_z += 1.0f; */

        /* // Keep angles bounded to prevent float overflow */
        /* if (angle_x >= 360.0f) angle_x -= 360.0f; */
        /* if (angle_y >= 360.0f) angle_y -= 360.0f; */
        /* if (angle_z >= 360.0f) angle_z -= 360.0f; */

        // 4. Blast to the OLED
        /* apply_global_dither(shadow_fb); */
        /* add_film_grain(shadow_fb, 5); */
        display_flush_buffer(shadow_fb);
        usleep(16000); // ~60 FPS
        frames_spent += 1;
    }
    memset(bg_fb, 0, FB_SIZE);
    memset(shadow_fb, 0, FB_SIZE);
    display_flush_buffer(shadow_fb);

}

void init_buttons() {
    // Open the device in NON-BLOCKING mode.
    // This is critical so your UI thread doesn't freeze waiting for a button press.
    input_fd = open(INPUT_DEVICE, O_RDONLY | O_NONBLOCK);
    if (input_fd < 0) {
        printf("Error: Could not open hardware buttons.\n");
    }
}

bool load_track(const char* filepath) {
    FILE* file = fopen(filepath, "rb");
    if (!file) return false;

    // 1. Clear out any previous track's waveform memory
    if (active_deck.visual_peaks != NULL) {
        free(active_deck.visual_peaks);
        active_deck.visual_peaks = NULL;
    }

    // 2. Route to your existing parsers based on extension
    bool parsed = false;
    if (strstr(filepath, ".wav") != NULL) {
        parsed = parse_wav_header(file, &active_deck.metadata);
    } else if (strstr(filepath, ".aiff") != NULL || strstr(filepath, ".aif") != NULL) {
        parsed = parse_aiff_header(file, &active_deck.metadata);
    }

    if (!parsed) {
        fclose(file);
        return false;
    }

    // 3. Extract just the filename for the UI
    const char* filename = strrchr(filepath, '/');
    filename = (filename) ? filename + 1 : filepath;
    strncpy(active_deck.track_name, filename, MAX_FILENAME_LEN - 1);

    // 4. Generate the UI Waveform Array
    /* active_deck.num_peaks = WAVEFORM_UI_WIDTH; */
    active_deck.num_peaks = active_deck.metadata.total_samples / ZOOM_SAMPLES_PER_PIXEL;
    active_deck.visual_peaks = (uint8_t*)malloc(active_deck.num_peaks);

    // Calculate how many audio samples represent 1 pixel of width
    /* uint32_t samples_per_pixel = active_deck.metadata.total_samples / WAVEFORM_UI_WIDTH; */
    int bytes_per_sample = active_deck.metadata.bit_depth / 8;
    int frame_size = active_deck.metadata.channels * bytes_per_sample;

    // Jump to the audio data
    fseek(file, active_deck.metadata.data_start_offset, SEEK_SET);

    // Read through the file and grab the peak volume for each pixel column
    uint8_t sample_buffer[8192];
    /* for (uint32_t i = 0; i < active_deck.num_peaks; i++) { */

    /*     uint32_t samples_to_read = samples_per_pixel; */
    /*     int32_t max_val = 0; */

    /*     while (samples_to_read > 0) { */
    /*         int chunk = (samples_to_read > 1024) ? 1024 : samples_to_read; */
    /*         size_t bytes_read = fread(sample_buffer, 1, chunk * frame_size, file); */
    /*         if (bytes_read == 0) break; */

    /*         // Simplified peak detection (casting raw bytes to 16-bit for visual approximation) */
    /*         int num_samples_read = bytes_read / frame_size; */
    /*         for (int s = 0; s < num_samples_read; s++) { */
    /*             int byte_offset = s * frame_size; */
    /*             for(int b = 0; b < bytes_per_sample; b++) { */
    /*                 int32_t val = abs((int8_t)sample_buffer[byte_offset + b]); */
    /*                 if (val > max_val) { */
    /*                     max_val = val; */
    /*                 } */
    /*             } */
    /*         } */
    /*         samples_to_read -= num_samples_read; */
    /*     } */

    /*     // Scale the audio peak down to the OLED pixel height (0 to 28 pixels) */
    /*     // Adjust the 32768 divisor depending on how "loud" you want the UI to look */
    /*     active_deck.visual_peaks[i] = (uint8_t)((max_val * WAVEFORM_MAX_HEIGHT) / 128); */
    /*     if (active_deck.visual_peaks[i] > WAVEFORM_MAX_HEIGHT) { */
    /*         active_deck.visual_peaks[i] = WAVEFORM_MAX_HEIGHT; */
    /*     } */
    /* } */

    // Determine if we are reading Little-Endian (WAV) or Big-Endian (AIFF)
    // to find where the Most Significant Byte (MSB) lives in the frame
    bool is_wav = (strstr(filepath, ".wav") != NULL || strstr(filepath, ".WAV") != NULL);

    for (uint32_t i = 0; i < active_deck.num_peaks; i++) {

        // Use the fixed zoom constant instead of calculating it per-screen
        uint32_t samples_to_read = ZOOM_SAMPLES_PER_PIXEL;
        /* uint32_t samples_to_read = samples_per_pixel; */
        uint64_t msb_sum = 0;
        uint32_t msb_count = 0;

        while (samples_to_read > 0) {
            int chunk = (samples_to_read > 1024) ? 1024 : samples_to_read;

            size_t bytes_read = fread(sample_buffer, 1, chunk * frame_size, file);
            if (bytes_read == 0) break;

            int num_samples_read = bytes_read / frame_size;

            for (int s = 0; s < num_samples_read; s++) {
                int byte_offset = s * frame_size;

                // Grab the Most Significant Byte of the Left Channel
                // WAV (Little-Endian): MSB is the last byte of the sample
                // AIFF (Big-Endian): MSB is the first byte of the sample
                int msb_index = is_wav ? (byte_offset + bytes_per_sample - 1) : byte_offset;

                // Cast to signed 8-bit to get amplitude (-128 to +127), then absolute value
                msb_sum += abs((int8_t)sample_buffer[msb_index]);
                msb_count++;
            }
            samples_to_read -= num_samples_read;
        }

        // Calculate the average amplitude for this pixel bucket
        int avg_val = (msb_count > 0) ? (msb_sum / msb_count) : 0;

        // Scale to OLED height.
        // A heavily compressed dance track will average an MSB around 40-60 out of 127.
        // Dividing by 45 scales those loud parts perfectly to the edges of the OLED.
        active_deck.visual_peaks[i] = (uint8_t)((avg_val * WAVEFORM_MAX_HEIGHT) / 45);

        // Safety cap so the waveform never draws outside the frame boundaries
        if (active_deck.visual_peaks[i] > WAVEFORM_MAX_HEIGHT) {
            active_deck.visual_peaks[i] = WAVEFORM_MAX_HEIGHT;
        }
    }

    fclose(file);

    // 5. Initialize the playback state
    active_deck.play_state = DECK_CUED;
    active_deck.current_sample_pos = 0;
    active_deck.bpm = 124.0f; // Mock BPM until DSP is ready

    // Initialize ALSA and spawn the audio thread
    /* if (!init_alsa_engine(filepath)) { */
    /*     printf("Warning: ALSA engine failed to start.\n"); */
    /* } */

    // 6. Transition the UI
    current_app_state = APP_STATE_DECK;
    return true;
}

#define SAMPLES_PER_PIXEL_BIN 1024 // Adjust this to change waveform zoom level

bool generate_ui_waveform(FILE* file, deck_state_t* deck) {
    if (!file || !deck) return false;

    // Calculate how many peak bins we need to allocate
    deck->num_peaks = deck->metadata.total_samples / SAMPLES_PER_PIXEL_BIN;
    deck->visual_peaks = (uint8_t*)malloc(deck->num_peaks);

    if (!deck->visual_peaks) return false;

    // Jump to the start of the actual PCM audio
    fseek(file, deck->metadata.data_start_offset, SEEK_SET);

    int32_t sample_buffer[SAMPLES_PER_PIXEL_BIN];
    int bytes_per_sample = deck->metadata.bit_depth / 8;
    int bytes_to_read = SAMPLES_PER_PIXEL_BIN * deck->metadata.channels * bytes_per_sample;

    for (uint32_t i = 0; i < deck->num_peaks; i++) {
        // Read a chunk of audio
        size_t bytes_read = fread(sample_buffer, 1, bytes_to_read, file);
        if (bytes_read == 0) break;

        // Find the absolute peak in this chunk
        int32_t max_val = 0;
        int num_samples_read = bytes_read / (deck->metadata.channels * bytes_per_sample);

        for (int s = 0; s < num_samples_read; s++) {
            // Very simplified: assuming 24-bit audio in lower 3 bytes of 32-bit int
            int32_t val = abs(sample_buffer[s * deck->metadata.channels]);
            if (val > max_val) {
                max_val = val;
            }
        }

        // Normalize the peak down to an 8-bit value (0-255) for easy UI drawing
        // Assuming 24-bit max value is 8,388,607
        deck->visual_peaks[i] = (uint8_t)((max_val * 255) / 8388607);
    }

    printf("Generated %d visual waveform peaks for the UI.\n", deck->num_peaks);
    return true;
}


void process_button_inputs() {
    if (input_fd < 0) return;

    struct input_event ev;

    // Read all pending events from the kernel buffer
    while (read(input_fd, &ev, sizeof(struct input_event)) > 0) {

        // We only care about Key presses (EV_KEY)
        if (ev.type == EV_KEY) {

            // ev.value: 0 = Released, 1 = Pressed, 2 = Auto-Repeat (Held down)
            if (ev.value == 1) {

                /* switch (ev.code) { */
                /*     case KEY_UP:     // SW1 */
                /*         update_browser_scroll(-1); // Move cursor up */
                /*         break; */

                /*     case KEY_DOWN:   // SW2 */
                /*         update_browser_scroll(1);  // Move cursor down */
                /*         break; */

                /*     case KEY_ENTER:  // SW3 */
                /*         int item_idx = cursor_index; */

                /*         if (current_directory[item_idx].type == ITEM_FOLDER) { */
                /*             // User clicked a folder: Build new path and scan it */

                /*             char temp_path[PATH_MAX + MAX_FILENAME_LEN + 2]; */
                /*             snprintf(temp_path, sizeof(temp_path), "%s/%s", current_path, current_directory[item_idx].name); */
                /*             char resolved_path[PATH_MAX]; */
                /*             if (realpath(temp_path, resolved_path) != NULL) { */
                /*                 // 3. Update the global path and rescan */
                /*                 strncpy(current_path, resolved_path, sizeof(current_path) - 1); */
                /*                 current_path[sizeof(current_path) - 1] = '\0'; */
                /*                 printf("Opening folder: %s\n", current_directory[item_idx].name); */
                /*                 scan_usb_drive(current_path); */
                /*             } else { */
                /*                 printf("Error: Could not resolve path %s\n", temp_path); */
                /*             } */
                /*         } else { */
                /*             // User clicked a track: Parse and Load it */
                /*             char track_path[PATH_MAX + MAX_FILENAME_LEN + 2]; */
                /*             snprintf(track_path, sizeof(track_path), "%s/%s", current_path, current_directory[item_idx].name); */

                /*             // Check extension and route to correct parser */
                /*             if (strstr(track_path, ".wav") != NULL) { */
                /*                 load_track(track_path); */
                /*             } else if (strstr(track_path, ".aiff") != NULL || strstr(track_path, ".aif") != NULL) { */
                /*                 load_track(track_path); */
                /*             } else { */
                /*                 printf("Unsupported format.\n"); */
                /*             } */
                /*         } */
                /* } */

                switch (ev.code) {
                    case KEY_UP:     // SW1
                        if (current_app_state == APP_STATE_BROWSER) {
                            update_browser_scroll(-1);
                        } else if (current_app_state == APP_STATE_DECK) {
                            // Toggle Play/Cue
                            if (active_deck.play_state == DECK_CUED) {
                                active_deck.play_state = DECK_PLAYING;
                            } else {
                                active_deck.play_state = DECK_CUED;
                            }
                        }
                        break;

                    case KEY_DOWN:   // SW2
                        if (current_app_state == APP_STATE_BROWSER) {
                            update_browser_scroll(1);
                        }
                        // TODO: Map to pitchbend or scrub in deck view
                        break;

                    case KEY_ENTER:  // SW3
                        if (current_app_state == APP_STATE_DECK) {
                            // Go back to browser
                            current_app_state = APP_STATE_BROWSER;
                        }
                        else if (current_app_state == APP_STATE_BROWSER) {
                            int item_idx = cursor_index;
                            if (current_directory[item_idx].type == ITEM_FOLDER) {
                                // (Keep your existing Folder routing code here)
                                char temp_path[PATH_MAX + MAX_FILENAME_LEN + 2];
                                snprintf(temp_path, sizeof(temp_path), "%s/%s", current_path, current_directory[item_idx].name);
                                char resolved_path[PATH_MAX];
                                if (realpath(temp_path, resolved_path) != NULL) {
                                    strncpy(current_path, resolved_path, sizeof(current_path) - 1);
                                    current_path[sizeof(current_path) - 1] = '\0';
                                    scan_usb_drive(current_path);
                                }
                            } else {
                                // LOAD THE TRACK
                                char track_path[PATH_MAX + MAX_FILENAME_LEN + 2];
                                snprintf(track_path, sizeof(track_path), "%s/%s", current_path, current_directory[item_idx].name);
                                load_track(track_path);
                            }
                        }
                        break;
                }

            }
        }
    }
}

int main() {
    srand(time(NULL));
    bool file_selected = false;

    display_init();
    init_buttons();
    debug_splash();
    cubes_animation_loop();
    // 1. Draw a main border around the whole screen
    draw_rect(bg_fb, 0, 0, FB_WIDTH, FB_HEIGHT, 1);
    memcpy(shadow_fb, bg_fb, FB_SIZE);

    const char* usb_root = "/mnt/usb";
    scan_usb_drive(usb_root);
    /* while (1) { */
    /*     process_button_inputs(); */
    /*     draw_browser(); */
    /*     usleep(16000); */
    /* } */

    while (1) {
        process_button_inputs();

        if (current_app_state == APP_STATE_BROWSER) {
            draw_browser();
        } else if (current_app_state == APP_STATE_DECK) {

            /* // Mock Playhead Advancement for testing the UI */
            /* if (active_deck.play_state == DECK_PLAYING) { */
            /*     // Advance playhead by a chunk of samples (e.g. 48000hz / 60fps = 800 samples per frame) */
            /*     active_deck.current_sample_pos += 800; */

            /*     if (active_deck.current_sample_pos >= active_deck.metadata.total_samples) { */
            /*         active_deck.current_sample_pos = 0; // Loop or stop */
            /*         active_deck.play_state = DECK_CUED; */
            /*     } */
            /* } */

            draw_deck();
        }
        usleep(16000); // 60 FPS cap
    }

    /* // header divider line */
    /* draw_line(0, 12, FB_WIDTH, 12, 1); */

    /* // header */
    /* draw_string(4, 4, "Please select a file."); */

    /* // test strings */
    /* draw_string(4, 18, "doodoo123.aiff"); */
    /* draw_string(4, 25, "doodoo (VIP).aiff"); */
    /* draw_string(4, 32, "micheal jacson.aiff"); */
    /* draw_string(4, 39, "bootyslims.aiff"); */

    /* // selection outline */
    /* draw_rect(3, 17, 122, 8, 1); */

    /* int x = 0; */
    /* int y = 0; */
    /* double last_time = get_time_in_seconds(); */
    /* while (x < FB_WIDTH) { */
    /*   /\* double current_time = get_time_in_seconds(); *\/ */
    /*   /\* double delta_time = current_time - last_time; // Time since last frame *\/ */
    /*   /\* last_time = current_time; *\/ */
    /*   memset(shadow_fb, 0, FB_SIZE); */
    /*   draw_pixel(shadow_fb, x, y, 1); */
    /*   display_flush_buffer(shadow_fb); */
    /*   usleep(2000); */
    /*   draw_pixel(shadow_fb, x, y, 0); */
    /*   display_flush_buffer(shadow_fb); */

    /*   /\* float progress = delta_time / FB_WIDTH; // 0.0 at start, 1.0 at end *\/ */
    /*   x += 1; */
    /* } */

    /* // 5. Fill a portion of the meter (e.g., audio peaking at 75%) */
    /* draw_filled_rect(4, 20, 75, 10, 1); */

    /* // 6. Draw some UI metric boxes */
    /* draw_rect(4, 38, 30, 16, 1); */
    /* draw_string(8, 43, "EQ"); */

    /* draw_rect(40, 38, 30, 16, 1); */
    /* draw_string(44, 43, "FX"); */

    /* printf("UI rendered to framebuffer.\n"); */
    display_cleanup();
    return 0;
}
