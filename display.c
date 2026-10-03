#include "display.h"
#include "gfx.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

static uint8_t *fb = NULL;
static int fbfd = -1;

void display_init() {

    fbfd = open("/dev/fb0", O_RDWR);
    /* if (fbfd < 0) { perror("Could not open /dev/fb0"); return 1; } */

    fb = (uint8_t *)mmap(0, FB_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    /* if (fbfd == MAP_FAILED) { perror("mmap failed"); return 1; } */

    /* struct fb_var_screeninfo vinfo; */
    /* if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == 0) { */
    /*   // Force the visible window to the top-left of the memory buffer */
    /*   vinfo.xoffset = 0; */
    /*   vinfo.yoffset = 0; */
    /*   ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo); */
    /* } */
}

void display_flush_buffer(uint8_t *buf) {
    if (fb != NULL) {
        memcpy(fb, buf, FB_SIZE);
    }
}

void display_cleanup() {
    if (fb != NULL) {
        memset(fb, 0, FB_SIZE);
    }
    if (fb != NULL) {
        munmap(fb, FB_SIZE);
        fb = NULL;
    }
    if (fbfd != -1) {
        close(fbfd);
        fbfd = -1;
    }
}
