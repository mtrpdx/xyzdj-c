#ifndef AUDIO_H_
#define AUDIO_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILENAME_LEN 64 // Truncate long names to save memory


/* bool load_wav_file(const char* filepath); */


// The unified data format your DSP engine expects
typedef struct __attribute__((packed)) {
    uint32_t total_samples;
    uint32_t sample_rate;
    uint16_t bit_depth;
    uint16_t channels;
    uint32_t data_start_offset; // Where the actual PCM audio begins in the file
} track_metadata_t;

typedef enum {
    DECK_CUED,
    DECK_PLAYING
} playback_state_t;


typedef struct {
    char track_name[MAX_FILENAME_LEN];
    float bpm;

    // 2. Playback State
    playback_state_t play_state;
    uint32_t current_sample_pos; // Where the playhead is currently sitting

    // 3. UI Waveform Data
    // We cannot render 44,100 samples per second to a screen.
    // We store a drastically downsampled "peak" array just for the UI.
    uint8_t* visual_peaks;
    uint32_t num_peaks;

    track_metadata_t metadata;

} deck_state_t;


extern deck_state_t active_deck;

// The master function your UI will call
/* bool load_track(const char* filepath); */

#endif // AUDIO_H_
