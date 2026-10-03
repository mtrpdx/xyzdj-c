#ifndef WAV_H_
#define WAV_H_

#include "audio.h"
#include <stdio.h>
#include <stdbool.h>



bool parse_wav_header(FILE* file, track_metadata_t* metadata);

#endif // WAV_H_
