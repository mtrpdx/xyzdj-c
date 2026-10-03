#include "wav.h"
#include <stdint.h>
#include <string.h>

typedef struct __attribute__((packed)) {
    char riff_header[4];      // Contains "RIFF"
    uint32_t wav_size;        // Size of the wav portion
    char wave_header[4];      // Contains "WAVE"

    char fmt_header[4];       // Contains "fmt "
    uint32_t fmt_chunk_size;  // Should be 16 for PCM
    uint16_t audio_format;    // Should be 1 for PCM
    uint16_t num_channels;    // 1 = Mono, 2 = Stereo
    uint32_t sample_rate;     // e.g., 44100 or 48000
    uint32_t byte_rate;       // Number of bytes per second
    uint16_t sample_alignment;// num_channels * Bytes Per Sample
    uint16_t bit_depth;       // 16, 24, or 32

    char data_header[4];      // Contains "data"
    uint32_t data_bytes;      // Number of bytes in the actual audio payload
} wav_header_t;


bool parse_wav_header(FILE* file, track_metadata_t* metadata) {
    /* // 1. Close any currently playing track */
    /* if (current_audio_file != NULL) { */
    /*     fclose(current_audio_file); */
    /*     current_audio_file = NULL; */
    /* } */

    /* // 2. Open the new file in binary read mode */
    /* current_audio_file = fopen(filepath, "rb"); */
    /* if (current_audio_file == NULL) { */
    /*     printf("Error: Could not open %s\n", filepath); */
    /*     return false; */
    /* } */

    wav_header_t header;

    // Read the 44-byte header
    if (fread(&header, 1, sizeof(wav_header_t), file) < sizeof(wav_header_t)) {
        return false;
    }

    // Validate magic bytes
    if (strncmp(header.riff_header, "RIFF", 4) != 0 ||
        strncmp(header.wave_header, "WAVE", 4) != 0) {
        return false;
    }

    if (header.audio_format != 1) {
        printf("Error: Only standard uncompressed PCM audio is supported.\n");
        return false;
    }


    // Translate the WAV struct into our unified metadata struct
    metadata->channels = header.num_channels;
    metadata->bit_depth = header.bit_depth;
    metadata->sample_rate = header.sample_rate;

    int bytes_per_sample = metadata->bit_depth / 8;
    metadata->total_samples = header.data_bytes / (metadata->channels * bytes_per_sample);

    // Save the exact byte offset where the PCM audio starts (useful for looping later)
    metadata->data_start_offset = ftell(file);

    printf("Parsed WAV: %d Hz, %d-bit, %d Channels\n",
           metadata->sample_rate, metadata->bit_depth, metadata->channels);

    return true; // File pointer is now sitting perfectly at the audio data
}
