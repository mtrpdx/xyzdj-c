#include "audio.h"
#include "wav.h"
#include "aiff.h"
#include "stdbool.h"
#include <string.h>
#include <stdio.h>

// // Track Data State
/* FILE* current_audio_file = NULL; */
/* uint32_t track_total_samples = 0; */
/* uint32_t track_samples_played = 0; */
/* uint16_t track_channels = 2; */
/* uint16_t track_bit_depth = 16; */

// The master state
FILE* current_audio_file = NULL;
track_metadata_t current_track;



/* bool load_track(const char* filepath) { */
/*     if (current_audio_file != NULL) { */
/*         fclose(current_audio_file); */
/*     } */

/*     current_audio_file = fopen(filepath, "rb"); */
/*     if (!current_audio_file) return false; */

/*     bool success = false; */

/*     // Route to the specialized module */
/*     if (strstr(filepath, ".wav") != NULL) { */
/*         success = parse_wav_header(current_audio_file, &current_track); */
/*     } */
/*     else if (strstr(filepath, ".aiff") != NULL || strstr(filepath, ".aif") != NULL) { */
/*         success = parse_aiff_header(current_audio_file, &current_track); */
/*     } */

/*     if (!success) { */
/*         fclose(current_audio_file); */
/*         current_audio_file = NULL; */
/*     } */

/*     return success; */
/* } */

