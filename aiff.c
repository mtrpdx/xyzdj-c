#include "aiff.h"
#include <stdint.h>
#include <string.h>

// AIFF Master Header
typedef struct __attribute__((packed)) {
    char form_id[4];      // "FORM"
    uint32_t form_size;   // Needs byte-swapping
    char aiff_id[4];      // "AIFF"
} aiff_header_t;

// Generic Chunk Header for scanning
typedef struct __attribute__((packed)) {
    char chunk_id[4];
    uint32_t chunk_size;  // Needs byte-swapping
} aiff_chunk_t;

// The COMM (Common) Chunk Payload
typedef struct __attribute__((packed)) {
    int16_t num_channels;       // Needs byte-swapping
    uint32_t num_sample_frames; // Needs byte-swapping
    int16_t bit_depth;          // Needs byte-swapping
    uint8_t sample_rate[10];    // 80-bit float
} aiff_comm_t;

// Decodes the Apple 80-bit extended precision float into a standard 32-bit integer
static uint32_t decode_aiff_sample_rate(uint8_t* extended) {
    int exponent = ((extended[0] << 8) | extended[1]) & 0x7FFF;
    exponent -= 16383;
    uint32_t mantissa = (extended[2] << 24) | (extended[3] << 16) | (extended[4] << 8) | extended[5];
    return mantissa >> (31 - exponent);
}

bool parse_aiff_header(FILE* file, track_metadata_t* metadata) {
    aiff_header_t master;

    if (fread(&master, 1, sizeof(aiff_header_t), file) < sizeof(aiff_header_t)) {
        return false;
    }

    if (strncmp(master.form_id, "FORM", 4) != 0 || strncmp(master.aiff_id, "AIFF", 4) != 0) {
        return false;
    }

    bool found_comm = false;
    bool found_ssnd = false;
    aiff_chunk_t chunk;

    // Scan chunks to find COMM and SSND
    while (fread(&chunk, 1, sizeof(aiff_chunk_t), file) == sizeof(aiff_chunk_t)) {
        uint32_t chunk_size = __builtin_bswap32(chunk.chunk_size);
        uint32_t jump_size = chunk_size + (chunk_size % 2);

        if (strncmp(chunk.chunk_id, "COMM", 4) == 0) {
            aiff_comm_t comm;
            fread(&comm, 1, sizeof(aiff_comm_t), file);

            // Translate the AIFF struct into our unified metadata struct
            metadata->channels = __builtin_bswap16(comm.num_channels);
            metadata->bit_depth = __builtin_bswap16(comm.bit_depth);
            metadata->total_samples = __builtin_bswap32(comm.num_sample_frames);
            metadata->sample_rate = decode_aiff_sample_rate(comm.sample_rate);

            found_comm = true;
            fseek(file, jump_size - sizeof(aiff_comm_t), SEEK_CUR);

        } else if (strncmp(chunk.chunk_id, "SSND", 4) == 0) {
            uint32_t offset;
            uint32_t block_size;
            fread(&offset, 1, 4, file);
            fread(&block_size, 1, 4, file);

            fseek(file, __builtin_bswap32(offset), SEEK_CUR);

            // Save the exact byte offset where the AIFF PCM audio starts
            metadata->data_start_offset = ftell(file);
            found_ssnd = true;
            break;
        } else {
            fseek(file, jump_size, SEEK_CUR);
        }
    }

    if (!found_comm || !found_ssnd) {
        return false;
    }

    printf("Parsed AIFF: %d Hz, %d-bit, %d Channels\n",
           metadata->sample_rate, metadata->bit_depth, metadata->channels);

    return true; // File pointer is perfectly aligned to the payload
}
