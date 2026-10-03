##
# xyzdj
#
# @file
# @version 0.1
#
# ==============================================================================
# Compiler Settings
# ==============================================================================
CC ?= arm-adi_glibc-linux-gnueabi-gcc
CFLAGS ?= -g -mthumb -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a5 -fstack-protector-strong -O2 -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security -Werror=format-security -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64 --sysroot=/opt/adi-distro-glibc/5.0.0/sysroots/cortexa5t2hf-neon-adi_glibc-linux-gnueabi
LDFLAGS ?= -lm -lasound -lpthread

# ==============================================================================
# Project Configuration
# ==============================================================================
TARGET = xyzdj
SRCS = main.c display.c gfx.c engine3d.c audio.c aiff.c wav.c
OBJS = $(SRCS:.c=.o)
DEPS = display.h gfx.h engine3d.h mcufont.h audio.h aiff.h wav.h

# ==============================================================================
# Build Rules
# ==============================================================================
.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) ${CFLAGS} -o $@ $^ $(LDFLAGS) -lm -lasound -lpthread

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
# end
