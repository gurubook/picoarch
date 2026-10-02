#include <SDL.h>
#include "libretro.h"
#include "libpicofe/input.h"
#include "in_sdl2.h"
#include "main.h"
#include "util.h"
#include <unistd.h>
#include <sys/time.h>
#include "core.h"
#include "libpicofe/fonts.h"
#include "menu.h"
#include "plat.h"
#include "scale.h"

#define MAX_SAMPLE_RATE 65536

static const struct in_default_bind in_sdl2_defbinds[] = {
	{ KC_UP,        IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_UP },
	{ KC_DOWN,      IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_DOWN },
	{ KC_LEFT,      IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_LEFT },
	{ KC_RIGHT,     IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_RIGHT },
	{ KC_LCTRL,     IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_B },
	{ KC_SPACE,     IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_A },
	{ KC_LSHIFT,    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_X },
	{ KC_LALT,      IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_Y },
	{ KC_RETURN,    IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_START },
	{ KC_RCTRL,     IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_SELECT },
	{ KC_TAB,       IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_L },
	{ KC_BACKSPACE, IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_R },
	{ 'q',          IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_L2 },
	{ KC_BACKSLASH, IN_BINDTYPE_PLAYER12, RETRO_DEVICE_ID_JOYPAD_R2 },
	{ KC_ESC,       IN_BINDTYPE_EMU, EACTION_MENU },
	{ 0, 0, 0 }
};

const struct menu_keymap in_sdl2_key_map[] =
{
	{ KC_UP,        PBTN_UP },
	{ KC_DOWN,      PBTN_DOWN },
	{ KC_LEFT,      PBTN_LEFT },
	{ KC_RIGHT,     PBTN_RIGHT },
	{ KC_SPACE,     PBTN_MOK },
	{ KC_LCTRL,     PBTN_MBACK },
	{ KC_LALT,      PBTN_MA2 },
	{ KC_LSHIFT,    PBTN_MA3 },
	{ KC_TAB,       PBTN_L },
	{ KC_BACKSPACE, PBTN_R },
	{ KC_ESC,       PBTN_MENU },
};

static const struct mod_keymap in_sdl2_mod_keymap[] = {
	{ KC_TAB,       'q' },           /* mod+L1 = L2 */
	{ KC_BACKSPACE, KC_BACKSLASH },  /* mod+R1 = R2 */
};

static const struct in_pdata in_sdl2_platform_data = {
	.defbinds     = in_sdl2_defbinds,
	.key_map      = in_sdl2_key_map,
	.kmap_size    = array_size(in_sdl2_key_map),
	.mod_key      = KC_ESC,
	.mod_keymap   = in_sdl2_mod_keymap,
	.modmap_size  = array_size(in_sdl2_mod_keymap),
};

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_Surface *screen;

struct audio_state {
	unsigned buf_w;
	unsigned max_buf_w;
	unsigned buf_r;
	size_t buf_len;
	struct audio_frame *buf;
	int in_sample_rate;
	int out_sample_rate;
	int sample_rate_adj;
	int adj_out_sample_rate;
};

struct audio_state audio;

static void plat_sound_select_resampler(void);
void (*plat_sound_write)(const struct audio_frame *data, int frames);

#define DRC_MAX_ADJUSTMENT 0.003
#define DRC_ADJ_BELOW 40
#define DRC_ADJ_ABOVE 60

static char msg[HUD_LEN];
static unsigned msg_priority = 0;
static unsigned msg_expire = 0;

static bool frame_dirty = false;
static int frame_time = 1000000 / 60;

static uint64_t plat_get_ticks_us_u64(void) {
	uint64_t ret;
	struct timeval tv;

	gettimeofday(&tv, NULL);

	ret = (uint64_t)tv.tv_sec * 1000000;
	ret += (uint64_t)tv.tv_usec;

	return ret;
}

static void video_expire_msg(void)
{
	msg[0] = '\0';
	msg_priority = 0;
	msg_expire = 0;
}

static void video_update_msg(void)
{
	if (msg[0] && msg_expire < plat_get_ticks_ms())
		video_expire_msg();
}

static void video_clear_msg(uint16_t *dst, uint32_t h, uint32_t pitch)
{
	memset(dst + (h - 10) * pitch, 0, 10 * pitch * sizeof(uint16_t));
}

static void video_print_msg(uint16_t *dst, uint32_t h, uint32_t pitch, char *msg)
{
	basic_text_out16_nf(dst, pitch, 2, h - 10, msg);
}

static int audio_resample_passthrough(struct audio_frame data) {
	audio.buf[audio.buf_w++] = data;
	if (audio.buf_w >= audio.buf_len) audio.buf_w = 0;

	return 1;
}

static int audio_resample_nearest(struct audio_frame data) {
	static int diff = 0;
	int consumed = 0;

	if (diff < audio.adj_out_sample_rate) {
		audio.buf[audio.buf_w++] = data;
		if (audio.buf_w >= audio.buf_len) audio.buf_w = 0;

		diff += audio.in_sample_rate;
	}

	if (diff >= audio.adj_out_sample_rate) {
		consumed++;
		diff -= audio.adj_out_sample_rate;
	}

	return consumed;
}

/*
 * Upload the composed 16-bit surface to the streamed texture and present.
 * On PicoCalc the SDL2 KMSDRM driver blits this straight to the 320x320
 * dumb buffer of the SPI LCD (no GPU, no kernel console involved).
 */
static void *fb_flip(void)
{
	SDL_UpdateTexture(texture, NULL, screen->pixels, screen->pitch);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
	return screen->pixels;
}

void *plat_prepare_screenshot(int *w, int *h, int *bpp)
{
	if (w) *w = SCREEN_WIDTH;
	if (h) *h = SCREEN_HEIGHT;
	if (bpp) *bpp = SCREEN_BPP;

	return screen->pixels;
}

int plat_dump_screen(const char *filename) {
	char imgname[MAX_PATH];
	int ret = -1;
	SDL_Surface *surface = NULL;

	snprintf(imgname, MAX_PATH, "%s.bmp", filename);

	if (g_menuscreen_ptr) {
		surface = SDL_CreateRGBSurfaceFrom(g_menubg_src_ptr,
		                                   g_menubg_src_w,
		                                   g_menubg_src_h,
		                                   16,
		                                   g_menubg_src_w * sizeof(uint16_t),
		                                   0xF800, 0x07E0, 0x001F, 0x0000);
		if (surface) {
			ret = SDL_SaveBMP(surface, imgname);
			SDL_FreeSurface(surface);
		}
	} else {
		ret = SDL_SaveBMP(screen, imgname);
	}

	return ret;
}

int plat_load_screen(const char *filename, void *buf, size_t buf_size, int *w, int *h, int *bpp) {
	int ret = -1;
	char imgname[MAX_PATH];
	SDL_Surface *imgsurface = NULL;
	SDL_Surface *surface = NULL;

	snprintf(imgname, MAX_PATH, "%s.bmp", filename);
	imgsurface = SDL_LoadBMP(imgname);
	if (!imgsurface)
		goto finish;

	/* convert to the same 16bpp format as the screen surface */
	{
		SDL_PixelFormat *fmt = SDL_AllocFormat(SDL_PIXELFORMAT_RGB565);
		surface = fmt ? SDL_ConvertSurface(imgsurface, fmt, 0) : NULL;
		if (fmt)
			SDL_FreeFormat(fmt);
	}
	if (!surface)
		goto finish;

	if (surface->pitch > SCREEN_PITCH ||
	    surface->h > SCREEN_HEIGHT ||
	    surface->w == 0 ||
	    surface->h * surface->pitch > buf_size)
		goto finish;

	memcpy(buf, surface->pixels, surface->pitch * surface->h);
	*w = surface->w;
	*h = surface->h;
	*bpp = surface->pitch / surface->w;

	ret = 0;

finish:
	if (imgsurface)
		SDL_FreeSurface(imgsurface);
	if (surface)
		SDL_FreeSurface(surface);
	return ret;
}


void plat_video_menu_enter(int is_rom_loaded)
{
	if (g_menuscreen_ptr)
		return;

	SDL_LockSurface(screen);
	memcpy(g_menubg_src_ptr, screen->pixels, g_menubg_src_h * g_menubg_src_pp * sizeof(uint16_t));
	SDL_UnlockSurface(screen);
	g_menuscreen_ptr = fb_flip();
}

void plat_video_menu_begin(void)
{
	SDL_LockSurface(screen);
	menu_begin();
}

void plat_video_menu_end(void)
{
	menu_end();
	SDL_UnlockSurface(screen);
	g_menuscreen_ptr = fb_flip();
}

void plat_video_menu_leave(void)
{
	memset(g_menubg_src_ptr, 0, g_menuscreen_h * g_menuscreen_pp * sizeof(uint16_t));

	SDL_LockSurface(screen);
	memset(screen->pixels, 0, g_menuscreen_h * g_menuscreen_pp * sizeof(uint16_t));
	SDL_UnlockSurface(screen);
	fb_flip();
	SDL_LockSurface(screen);
	memset(screen->pixels, 0, g_menuscreen_h * g_menuscreen_pp * sizeof(uint16_t));
	SDL_UnlockSurface(screen);

	g_menuscreen_ptr = NULL;
}

void plat_video_open(void)
{
}

void plat_video_set_msg(const char *new_msg, unsigned priority, unsigned msec)
{
	if (!new_msg) {
		video_expire_msg();
	} else if (priority >= msg_priority) {
		snprintf(msg, HUD_LEN, "%s", new_msg);
		string_truncate(msg, HUD_LEN - 1);
		msg_priority = priority;
		msg_expire = plat_get_ticks_ms() + msec;
	}
}

void plat_video_process(const void *data, unsigned width, unsigned height, size_t pitch) {
	static int had_msg = 0;
	frame_dirty = true;
	SDL_LockSurface(screen);

	if (had_msg) {
		video_clear_msg(screen->pixels, screen->h, screen->pitch / SCREEN_BPP);
		had_msg = 0;
	}

	scale(width, height, pitch, data, screen->pixels);

	if (msg[0]) {
		video_print_msg(screen->pixels, screen->h, screen->pitch / SCREEN_BPP, msg);
		had_msg = 1;
	}

	SDL_UnlockSurface(screen);

	video_update_msg();
}

void plat_video_flip(void)
{
	static uint64_t next_frame_time_us = 0;

	if (frame_dirty) {
		if (enable_drc) {
			uint64_t time = plat_get_ticks_us_u64();

			if (limit_frames && time < next_frame_time_us) {
				uint32_t delaytime = (next_frame_time_us - time - 1) / 1000 + 1;

				if (delaytime < 1000)
					SDL_Delay(delaytime);
				else
					next_frame_time_us = 0;

				time = plat_get_ticks_us_u64();
			}

			if (!next_frame_time_us || !limit_frames) {
				next_frame_time_us = time;
			}

			fb_flip();

			do {
				next_frame_time_us += frame_time;
			} while (next_frame_time_us < time);
		} else {
			fb_flip();
			next_frame_time_us = 0;
		}

		frame_dirty = false;
	}
}

void plat_video_close(void)
{
}

unsigned plat_cpu_ticks(void)
{
	long unsigned ticks = 0;
	long ticksps = 0;
	FILE *file = NULL;

	file = fopen("/proc/self/stat", "r");
	if (!file)
		goto finish;

	if (!fscanf(file, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu", &ticks))
		goto finish;

	ticksps = sysconf(_SC_CLK_TCK);

	if (ticksps)
		ticks = ticks * 100 / ticksps;

finish:
	if (file)
		fclose(file);

	return ticks;
}

static void plat_sound_callback(void *unused, uint8_t *stream, int len)
{
	int16_t *p = (int16_t *)stream;
	if (audio.buf_len == 0)
		return;

	len /= (sizeof(int16_t) * 2);

	while (audio.buf_r != audio.buf_w && len > 0) {
		*p++ = audio.buf[audio.buf_r].left;
		*p++ = audio.buf[audio.buf_r].right;
		audio.max_buf_w = audio.buf_r;

		len--;
		audio.buf_r++;

		if (audio.buf_r >= audio.buf_len) audio.buf_r = 0;
	}

	while(len > 0) {
		*p++ = 0;
		--len;
	}
}

static void plat_sound_finish(void)
{
	SDL_PauseAudio(1);
	SDL_CloseAudio();
	if (audio.buf) {
		free(audio.buf);
		audio.buf = NULL;
	}
}

static int plat_sound_init(void)
{
	/* audio subsystem already initialised in plat_init() */
	SDL_AudioSpec spec, received;

	spec.freq = MIN(sample_rate, MAX_SAMPLE_RATE);
	spec.format = AUDIO_S16LSB;
	spec.channels = 2;
	spec.samples = 512;
	spec.callback = plat_sound_callback;

	if (SDL_OpenAudio(&spec, &received) < 0) {
		plat_sound_finish();
		return -1;
	}

	audio.in_sample_rate = sample_rate;
	audio.out_sample_rate = received.freq;
	audio.sample_rate_adj = audio.out_sample_rate * DRC_MAX_ADJUSTMENT;
	audio.adj_out_sample_rate = audio.out_sample_rate;

	plat_sound_select_resampler();
	plat_sound_resize_buffer();

	SDL_PauseAudio(0);
	return 0;
}

int plat_sound_occupancy(void)
{
	int buffered = 0;
	if (audio.buf_len == 0)
		return 0;

	if (audio.buf_w != audio.buf_r) {
		buffered = audio.buf_w > audio.buf_r ?
			audio.buf_w - audio.buf_r :
			(audio.buf_w + audio.buf_len) - audio.buf_r;
	}

	return buffered * 100 / audio.buf_len;
}

#define BATCH_SIZE 100
void plat_sound_write_resample(const struct audio_frame *data, int frames, int (*resample)(struct audio_frame data), bool drc)
{
	int consumed = 0;
	if (audio.buf_len == 0)
		return;

	if (drc) {
		int occupancy = plat_sound_occupancy();

		if (occupancy < DRC_ADJ_BELOW) {
			audio.adj_out_sample_rate = audio.out_sample_rate + audio.sample_rate_adj;
		} else if (occupancy > DRC_ADJ_ABOVE) {
			audio.adj_out_sample_rate = audio.out_sample_rate - audio.sample_rate_adj;
		} else {
			audio.adj_out_sample_rate = audio.out_sample_rate;
		}
	}

	SDL_LockAudio();

	while (frames > 0) {
		int tries = 0;
		int amount = MIN(BATCH_SIZE, frames);

		while (tries < 10 && audio.buf_w == audio.max_buf_w) {
			tries++;
			SDL_UnlockAudio();

			if (!limit_frames)
				return;

			plat_sleep_ms(1);
			SDL_LockAudio();
		}

		while (amount && audio.buf_w != audio.max_buf_w) {
			consumed = resample(*data);
			data += consumed;
			amount -= consumed;
			frames -= consumed;
		}
	}
	SDL_UnlockAudio();
}

void plat_sound_write_passthrough(const struct audio_frame *data, int frames)
{
	plat_sound_write_resample(data, frames, audio_resample_passthrough, false);
}

void plat_sound_write_nearest(const struct audio_frame *data, int frames)
{
	plat_sound_write_resample(data, frames, audio_resample_nearest, false);
}

void plat_sound_write_drc(const struct audio_frame *data, int frames)
{
	plat_sound_write_resample(data, frames, audio_resample_nearest, true);
}

void plat_sound_resize_buffer(void) {
	size_t buf_size;
	SDL_LockAudio();

	audio.buf_len = frame_rate > 0
		? current_audio_buffer_size * audio.in_sample_rate / frame_rate
		: 0;

		/* Dynamic adjustment keeps buffer 50% full, need double size */
	if (enable_drc)
		audio.buf_len *= 2;

	if (audio.buf_len == 0) {
		SDL_UnlockAudio();
		return;
	}

	buf_size = audio.buf_len * sizeof(struct audio_frame);
	audio.buf = realloc(audio.buf, buf_size);

	if (!audio.buf) {
		SDL_UnlockAudio();
		PA_ERROR("Error initializing sound buffer\n");
		plat_sound_finish();
		return;
	}

	memset(audio.buf, 0, buf_size);
	audio.buf_w = 0;
	audio.buf_r = 0;
	audio.max_buf_w = audio.buf_len - 1;
	SDL_UnlockAudio();
}

static void plat_sound_select_resampler(void)
{
	if (enable_drc) {
		PA_INFO("Using audio adjustment (in: %d, out: %d-%d)\n", audio.in_sample_rate, audio.out_sample_rate - audio.sample_rate_adj, audio.out_sample_rate + audio.sample_rate_adj);
		plat_sound_write = plat_sound_write_drc;
	} else if (audio.in_sample_rate == audio.out_sample_rate) {
		PA_INFO("Using passthrough resampler (in: %d, out: %d)\n", audio.in_sample_rate, audio.out_sample_rate);
		plat_sound_write = plat_sound_write_passthrough;
	} else {
		PA_INFO("Using nearest resampler (in: %d, out: %d)\n", audio.in_sample_rate, audio.out_sample_rate);
		plat_sound_write = plat_sound_write_nearest;
	}
}

void plat_sdl2_event_handler(void *event_)
{
}

int plat_init(void)
{
	plat_sound_write = plat_sound_write_nearest;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		PA_ERROR("%s, SDL_Init failed: %s\n", __func__, SDL_GetError());
		return -1;
	}

	/*
	 * Request exactly the native panel size; the Calculinux SDL2 build's
	 * KMSDRM backend scales any mismatch down to the fixed 320x320 mode.
	 */
	window = SDL_CreateWindow("picoarch", 0, 0,
		SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_BORDERLESS);
	if (window == NULL) {
		PA_ERROR("%s, failed to create window: %s\n", __func__, SDL_GetError());
		goto fail;
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (renderer == NULL) {
		PA_ERROR("%s, failed to create renderer: %s\n", __func__, SDL_GetError());
		goto fail;
	}

	screen = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT,
		SCREEN_BPP * 8, SDL_PIXELFORMAT_RGB565);
	if (screen == NULL) {
		PA_ERROR("%s, failed to create screen surface: %s\n", __func__, SDL_GetError());
		goto fail;
	}

	texture = SDL_CreateTextureFromSurface(renderer, screen);
	if (texture == NULL) {
		PA_ERROR("%s, failed to create texture: %s\n", __func__, SDL_GetError());
		goto fail;
	}

	SDL_ShowCursor(0);

	g_menuscreen_w = SCREEN_WIDTH;
	g_menuscreen_h = SCREEN_HEIGHT;
	g_menuscreen_pp = SCREEN_WIDTH;
	g_menuscreen_ptr = NULL;

	g_menubg_src_w = SCREEN_WIDTH;
	g_menubg_src_h = SCREEN_HEIGHT;
	g_menubg_src_pp = SCREEN_WIDTH;

	if (in_sdl2_init(&in_sdl2_platform_data, plat_sdl2_event_handler)) {
		PA_ERROR("SDL2 input failed to init: %s\n", SDL_GetError());
		goto fail;
	}
	in_probe();

	if (plat_sound_init()) {
		PA_ERROR("SDL2 sound failed to init: %s\n", SDL_GetError());
		goto fail;
	}
	return 0;

fail:
	plat_finish();
	return -1;
}

int plat_reinit(void)
{
	if (sample_rate && sample_rate != audio.in_sample_rate) {
		plat_sound_finish();

		if (plat_sound_init()) {
			PA_ERROR("SDL sound failed to init: %s\n", SDL_GetError());
			return -1;
		}
	} else {
		plat_sound_resize_buffer();
		plat_sound_select_resampler();
	}

	if (frame_rate != 0)
		frame_time = 1000000 / frame_rate;

	scale_update_scaler();
	return 0;
}

void plat_finish(void)
{
	plat_sound_finish();

	if (texture) {
		SDL_DestroyTexture(texture);
		texture = NULL;
	}
	if (renderer) {
		SDL_DestroyRenderer(renderer);
		renderer = NULL;
	}
	if (window) {
		SDL_DestroyWindow(window);
		window = NULL;
	}

	SDL_FreeSurface(screen);
	screen = NULL;
	SDL_Quit();
}

