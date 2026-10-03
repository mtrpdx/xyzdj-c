#ifndef AIFF_H_
#define AIFF_H_

#include "audio.h"
#include "audio.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* bool load_aiff_file(const char* filepath); */
// It takes the file pointer and fills out the unified metadata struct
bool parse_aiff_header(FILE* file, track_metadata_t* metadata);

#endif // AIFF_H_
