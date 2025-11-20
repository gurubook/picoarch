#ifndef _MENU_H__
#define _MENU_H__

#include "config.h"
#include "libpicofe/menu.h"

static int menu_init(void);
static void menu_loop(void);
static int menu_select_core(void);
static int menu_select_content(char *filename, size_t len);
static void menu_begin(void);
static void menu_end(void);
static void menu_finish(void);

#endif
