/*
 * SDL2 based input driver for picoarch (Calculinux/PicoCalc).
 *
 * Ported from libpicofe/in_sdl.c (SDL 1.2), keeping the same in_drv_t
 * interface and key-binding semantics. The difference is the
 * event source: SDL2 keyboard events, which on PicoCalc are fed by the
 * SDL2 evdev driver (grabbing the keyboard while the app owns the KMSDRM
 * display) instead of a kernel virtual terminal, which does not exist on
 * Calculinux.
 *
 * (C) original in_sdl.c: Gražvydas "notaz" Ignotas, 2012
 * This file is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 *  - MAME license.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <string.h>
#include <SDL.h>

#include "in_sdl2.h"

#define IN_SDL2_PREFIX "sdl2:"
/* should be machine word for best performance */
typedef unsigned long keybits_t;
#define KEYBITS_WORD_BITS (sizeof(keybits_t) * 8)
#define KC_WORDS (KC_COUNT / KEYBITS_WORD_BITS + 1)

struct in_sdl2_state {
	const in_drv_t *drv;
	keybits_t keystate[KC_WORDS];
	// emulator keys should always be processed immediately lest one is lost
	keybits_t emu_keys[KC_WORDS];
};

static void (*ext_event_handler)(void *event);

/* key names, indexed by our compact keycodes */
static char kc_char_names[256][2];
static const char * const kc_named_names[] = {
	[KC_ESC - 256]         = "escape",
	[KC_RETURN - 256]      = "return",
	[KC_TAB - 256]         = "tab",
	[KC_BACKSPACE - 256]   = "backspace",
	[KC_SPACE - 256]       = "space",
	[KC_UP - 256]          = "up",
	[KC_DOWN - 256]        = "down",
	[KC_LEFT - 256]        = "left",
	[KC_RIGHT - 256]       = "right",
	[KC_LSHIFT - 256]      = "left shift",
	[KC_RSHIFT - 256]      = "right shift",
	[KC_LCTRL - 256]       = "left ctrl",
	[KC_RCTRL - 256]       = "right ctrl",
	[KC_LALT - 256]        = "left alt",
	[KC_RALT - 256]        = "right alt",
	[KC_BACKSLASH - 256]   = "\\",
};
static const char *kc_names[KC_COUNT];

static void kc_names_init(void)
{
	static int initialized;
	int i;

	if (!initialized) {
		for (i = 32; i < 256; i++) {
			kc_char_names[i][0] = (char)i;
			kc_char_names[i][1] = '\0';
		}
		for (i = 0; i < KC_COUNT; i++)
			kc_names[i] = (i < 256) ? kc_char_names[i]
			                          : kc_named_names[i - 256];
		initialized = 1;
	}
}

/* map SDL2 keycode to our compact keycode domain, -1 when unsupported */
static int sdl2key_to_kc(SDL_Keycode sym)
{
	switch (sym) {
	case SDLK_ESCAPE:    return KC_ESC;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:  return KC_RETURN;
	case SDLK_TAB:       return KC_TAB;
	case SDLK_BACKSPACE: return KC_BACKSPACE;
	case SDLK_SPACE:     return KC_SPACE;
	case SDLK_UP:        return KC_UP;
	case SDLK_DOWN:      return KC_DOWN;
	case SDLK_LEFT:      return KC_LEFT;
	case SDLK_RIGHT:     return KC_RIGHT;
	case SDLK_LSHIFT:    return KC_LSHIFT;
	case SDLK_RSHIFT:    return KC_RSHIFT;
	case SDLK_LCTRL:     return KC_LCTRL;
	case SDLK_RCTRL:     return KC_RCTRL;
	case SDLK_LALT:      return KC_LALT;
	case SDLK_RALT:      return KC_RALT;
	case SDLK_BACKSLASH: return KC_BACKSLASH;
	default:
		/* printable Unicode keys: keycode is the character code */
		if (sym >= 32 && sym < 256)
			return sym;
		return -1;
	}
}

static void in_sdl2_probe(const in_drv_t *drv)
{
	struct in_sdl2_state *state;

	kc_names_init();

	state = calloc(1, sizeof(*state));
	if (state == NULL) {
		fprintf(stderr, "in_sdl2: OOM\n");
		return;
	}

	state->drv = drv;
	in_register(IN_SDL2_PREFIX "keys", -1, state, KC_COUNT, kc_names, 0);
}

static void in_sdl2_free(void *drv_data)
{
	struct in_sdl2_state *state = drv_data;

	if (state != NULL)
		free(state);
}

static const char * const *
in_sdl2_get_key_names(const in_drv_t *drv, int *count)
{
	*count = KC_COUNT;
	kc_names_init();
	return kc_names;
}

/* could use SDL_GetKeyState, but this gives better packing */
static void update_keystate(keybits_t *keystate, int sym, int is_down)
{
	keybits_t *ks_word, mask;

	mask = 1;
	mask <<= sym & (KEYBITS_WORD_BITS - 1);
	ks_word = keystate + sym / KEYBITS_WORD_BITS;
	if (is_down)
		*ks_word |= mask;
	else
		*ks_word &= ~mask;
}

static int get_keystate(keybits_t *keystate, int sym)
{
	keybits_t *ks_word, mask;

	mask = 1;
	mask <<= sym & (KEYBITS_WORD_BITS - 1);
	ks_word = keystate + sym / KEYBITS_WORD_BITS;
	return !!(*ks_word & mask);
}

static int handle_event(struct in_sdl2_state *state, SDL_Event *event,
	int *kc_out, int *down_out, int *emu_out)
{
	int kc, emu;

	if (event->type != SDL_KEYDOWN && event->type != SDL_KEYUP)
		return -1;

	kc = sdl2key_to_kc(event->key.keysym.sym);
	if (kc < 0)
		return -1;

	emu = get_keystate(state->emu_keys, kc);
	update_keystate(state->keystate, kc, event->type == SDL_KEYDOWN);
	if (kc_out != NULL)
		*kc_out = kc;
	if (down_out != NULL)
		*down_out = event->type == SDL_KEYDOWN;
	if (emu_out != 0)
		*emu_out = emu;

	return 1;
}

static int collect_events(struct in_sdl2_state *state, int *one_kc, int *one_down)
{
	int is_emukey;
	int i, ret, retval = 0;
	SDL_Event event;

	/* bounded drain so an event burst cannot stall a frame; events left
	 * in the SDL queue are picked up on the next call */
	for (i = 0; i < 64 && SDL_PollEvent(&event); i++) {
		ret = handle_event(state, &event,
			one_kc, one_down, &is_emukey);
		if (ret < 0) {
			if (ext_event_handler != NULL)
				ext_event_handler(&event);
			continue;
		}

		retval |= ret;
		if ((is_emukey || one_kc != NULL) && ret)
			break;
	}

	return retval;
}

static int in_sdl2_update(void *drv_data, const int *binds, int *result)
{
	struct in_sdl2_state *state = drv_data;
	keybits_t mask;
	int i, sym, bit, b;

	collect_events(state, NULL, NULL);

	for (i = 0; i < KC_WORDS; i++) {
		mask = state->keystate[i];
		if (mask == 0)
			continue;
		for (bit = 0; mask != 0; bit++, mask >>= 1) {
			if ((mask & 1) == 0)
				continue;
			sym = i * KEYBITS_WORD_BITS + bit;

			for (b = 0; b < IN_BINDTYPE_COUNT; b++)
				result[b] |= binds[IN_BIND_OFFS(sym, b)];
		}
	}

	return 0;
}

static int in_sdl2_update_keycode(void *drv_data, int *is_down)
{
	struct in_sdl2_state *state = drv_data;
	int ret_kc = -1, ret_down = 0;

	collect_events(state, &ret_kc, &ret_down);

	if (is_down != NULL)
		*is_down = ret_down;

	return ret_kc;
}

static int in_sdl2_menu_translate(void *drv_data, int keycode, char *charcode)
{
	struct in_sdl2_state *state = drv_data;
	const struct in_pdata *pdata = state->drv->pdata;
	const struct menu_keymap *map = pdata->key_map;
	int map_len = pdata->kmap_size;
	int ret = 0;
	int i;

	if (keycode < 0)
	{
		/* menu -> kc */
		keycode = -keycode;
		for (i = 0; i < map_len; i++)
			if (map[i].pbtn == keycode)
				return map[i].key;
	}
	else
	{
		for (i = 0; i < map_len; i++) {
			if (map[i].key == keycode) {
				ret = map[i].pbtn;
				break;
			}
		}

		if (charcode != NULL && keycode < 256 && keycode >= 32 &&
		    kc_names[keycode] != NULL && kc_names[keycode][1] == 0)
		{
			ret |= PBTN_CHAR;
			*charcode = kc_names[keycode][0];
		}
	}

	return ret;
}

static int in_sdl2_clean_binds(void *drv_data, int *binds, int *def_binds)
{
	struct in_sdl2_state *state = drv_data;
	int i, t, cnt = 0;

	memset(state->emu_keys, 0, sizeof(state->emu_keys));
	for (t = 0; t < IN_BINDTYPE_COUNT; t++)
		for (i = 0; i < KC_COUNT; i++)
			if (binds[IN_BIND_OFFS(i, t)]) {
				if (t == IN_BINDTYPE_EMU)
					update_keystate(state->emu_keys, i, 1);
				cnt ++;
			}

	return cnt;
}

static const in_drv_t in_sdl2_drv = {
	.prefix         = IN_SDL2_PREFIX,
	.probe          = in_sdl2_probe,
	.free           = in_sdl2_free,
	.get_key_names  = in_sdl2_get_key_names,
	.update         = in_sdl2_update,
	.update_keycode = in_sdl2_update_keycode,
	.menu_translate = in_sdl2_menu_translate,
	.clean_binds    = in_sdl2_clean_binds,
};

int in_sdl2_init(const struct in_pdata *pdata, void (*handler)(void *event))
{
	if (!pdata) {
		fprintf(stderr, "in_sdl2: Missing input platform data\n");
		return -1;
	}

	kc_names_init();

	in_register_driver(&in_sdl2_drv, pdata->defbinds, pdata);
	ext_event_handler = handler;
	return 0;
}
