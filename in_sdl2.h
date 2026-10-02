#ifndef __IN_SDL2_H__
#define __IN_SDL2_H__

#include "libpicofe/input.h"

/*
 * Compact keycode domain used by the SDL2 input driver.
 *
 * 0..255   printable Unicode keys (SDL2 keycodes are Unicode, so they are
 *          used directly as keycodes)
 * 256..    named / special keys
 */
enum {
	KC_ESC = 256,
	KC_RETURN,
	KC_TAB,
	KC_BACKSPACE,
	KC_SPACE,
	KC_UP,
	KC_DOWN,
	KC_LEFT,
	KC_RIGHT,
	KC_LSHIFT,
	KC_RSHIFT,
	KC_LCTRL,
	KC_RCTRL,
	KC_LALT,
	KC_RALT,
	KC_BACKSLASH,
	KC_COUNT
};

int in_sdl2_init(const struct in_pdata *pdata, void (*handler)(void *event));

#endif
