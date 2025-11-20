#ifndef __PLAT_H__
#define __PLAT_H__

#include "libpicofe/plat.h"

struct audio_frame {
	int16_t left;
	int16_t right;
};


static int  plat_init(void);
static int  plat_reinit(void);
static void plat_finish(void);
static void plat_minimize(void);

static void *plat_prepare_screenshot(int *w, int *h, int *bpp);
static int plat_dump_screen(const char *filename);
static int plat_load_screen(const char *filename, void *buf, size_t buf_size, int *w, int *h, int *bpp);

static void plat_video_open(void);
static void plat_video_set_msg(const char *new_msg, unsigned priority, unsigned msec);
static void plat_video_process(const void *data, unsigned width, unsigned height, size_t pitch);
static void plat_video_flip(void);
static void plat_video_close(void);

static unsigned plat_cpu_ticks(void);

static int plat_sound_occupancy(void);
extern void (*plat_sound_write)(const struct audio_frame *data, int frames);
static void plat_sound_resize_buffer(void);

#endif /* __PLAT_H__ */
