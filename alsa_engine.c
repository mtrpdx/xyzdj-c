#include "audio.h"
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

// Global ALSA handle and thread control
static snd_pcm_t *pcm_handle = NULL;
static pthread_t audio_thread_id;
static bool keep_audio_thread_alive = false;
static FILE* active_audio_file = NULL;


// The background thread that pushes bytes to the DAC
void* alsa_playback_thread(void* arg) {
    // We'll read 4096 frames at a time
    snd_pcm_uframes_t frames = 4096;
    int bytes_per_frame = active_deck.metadata.channels * (active_deck.metadata.bit_depth / 8);
    int buffer_size = frames * bytes_per_frame;

    uint8_t* pcm_buffer = (uint8_t*)malloc(buffer_size);

    while (keep_audio_thread_alive) {
        if (active_deck.play_state == DECK_PLAYING && active_audio_file != NULL) {

            // Read a chunk of raw PCM data from the USB drive
            size_t bytes_read = fread(pcm_buffer, 1, buffer_size, active_audio_file);

            if (bytes_read > 0) {
                // Advance the UI playhead cursor safely
                active_deck.current_sample_pos += (bytes_read / bytes_per_frame);

                // Push to ALSA. This function "blocks" (sleeps) until the hardware
                // is ready for more data, keeping perfect timing without a manual timer.
                int pcm_rc = snd_pcm_writei(pcm_handle, pcm_buffer, bytes_read / bytes_per_frame);

                // Handle buffer underruns (xruns) if the USB drive is too slow
                if (pcm_rc == -EPIPE) {
                    fprintf(stderr, "ALSA Underrun! Recovering...\n");
                    snd_pcm_prepare(pcm_handle);
                } else if (pcm_rc < 0) {
                    fprintf(stderr, "ALSA write error: %s\n", snd_strerror(pcm_rc));
                }
            } else {
                // End of file reached
                active_deck.play_state = DECK_CUED;
                active_deck.current_sample_pos = 0;
                fseek(active_audio_file, active_deck.metadata.data_start_offset, SEEK_SET);
            }
        } else {
            // Track is paused, sleep for a few milliseconds to save CPU
            usleep(10000);
        }
    }

    free(pcm_buffer);
    return NULL;
}

bool init_alsa_engine(const char* filepath) {
    int pcm_rc;

    // 1. Close any existing file/ALSA handle from a previous track
    if (active_audio_file) fclose(active_audio_file);
    if (pcm_handle) snd_pcm_close(pcm_handle);

    // 2. Open the file and jump to the raw audio data
    active_audio_file = fopen(filepath, "rb");
    if (!active_audio_file) return false;
    fseek(active_audio_file, active_deck.metadata.data_start_offset, SEEK_SET);

    // 3. Open the default ALSA hardware device for playback
    pcm_rc = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (pcm_rc < 0) {
        fprintf(stderr, "Cannot open ALSA device: %s\n", snd_strerror(pcm_rc));
        return false;
    }

    // 4. Configure the Hardware Parameters based on the WAV/AIFF header
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm_handle, params);

    // Interleaved data (L R L R)
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    // Match the Bit Depth
    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    if (active_deck.metadata.bit_depth == 24) {
        format = SND_PCM_FORMAT_S24_3LE; // Packed 24-bit
    } else if (active_deck.metadata.bit_depth == 32) {
        format = SND_PCM_FORMAT_S32_LE;
    }

    // Note: If playing an AIFF (Big Endian), you need SND_PCM_FORMAT_S16_BE, etc.
    // For now, assuming WAV (Little Endian) for testing.
    snd_pcm_hw_params_set_format(pcm_handle, params, format);
    snd_pcm_hw_params_set_channels(pcm_handle, params, active_deck.metadata.channels);

    unsigned int rate = active_deck.metadata.sample_rate;
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0);

    // Write parameters to the driver
    pcm_rc = snd_pcm_hw_params(pcm_handle, params);
    if (pcm_rc < 0) {
        fprintf(stderr, "Cannot set ALSA hardware params: %s\n", snd_strerror(pcm_rc));
        return false;
    }

    // 5. Boot the background thread if it isn't running
    if (!keep_audio_thread_alive) {
        keep_audio_thread_alive = true;
        pthread_create(&audio_thread_id, NULL, alsa_playback_thread, NULL);
    }

    return true;
}

void cleanup_alsa_engine() {
    keep_audio_thread_alive = false;
    pthread_join(audio_thread_id, NULL); // Wait for thread to exit

    if (pcm_handle) snd_pcm_drain(pcm_handle);
    if (pcm_handle) snd_pcm_close(pcm_handle);
    if (active_audio_file) fclose(active_audio_file);
}
