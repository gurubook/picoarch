#ifndef __PLAT_H__
#define __PLAT_H__

#include "libpicofe/plat.h"

struct audio_frame {
	int16_t left;
	int16_t right;
};

int  plat_init(void);
int  plat_reinit(void);
void plat_finish(void);
void plat_minimize(void);

void *plat_prepare_screenshot(int *w, int *h, int *bpp);
int plat_dump_screen(const char *filename);
int plat_load_screen(const char *filename, void *buf, size_t buf_size, int *w, int *h, int *bpp);

void plat_video_open(void);
void plat_video_set_msg(const char *new_msg, unsigned priority, unsigned msec);
void plat_video_process(const void *data, unsigned width, unsigned height, size_t pitch);
void plat_video_flip(void);
void plat_video_close(void);

unsigned plat_cpu_ticks(void);

int plat_sound_occupancy(void);
extern void (*plat_sound_write)(const struct audio_frame *data, int frames);
void plat_sound_resize_buffer(void);

#endif /* __PLAT_H__ */
