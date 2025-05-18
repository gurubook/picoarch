#include <sys/stat.h>
#include "core.h"
#include "main.h"
#include "menu.h"
#include "options.h"
#include "overrides.h"
#include "plat.h"
#include "scale.h"
#include "util.h"

#ifdef FUNKEY_S
#include "funkey/fk_instant_play.h"
#endif

static int drew_alt_bg = 0;

static char cores_path[MAX_PATH];
static struct dirent **corelist = NULL;
static int corelist_len = 0;

static const char *new_fname = NULL;

#define MENU_ALIGN_LEFT 0
#define MENU_X2 0

#define MENU_ITEMS_PER_PAGE 11

typedef enum
{
	MA_NONE = 1,
	MA_MAIN_RESUME_GAME,
	MA_MAIN_SAVE_STATE,
	MA_MAIN_LOAD_STATE,
	MA_MAIN_DISC_CTRL,
	MA_MAIN_CHEATS,
	MA_MAIN_CORE_SEL,
	MA_MAIN_CONTENT_SEL,
	MA_MAIN_RESET_GAME,
	MA_MAIN_CREDITS,
	MA_MAIN_EXIT,
	MA_OPT_CORE_OPTS,
	MA_OPT_SAVECFG,
	MA_OPT_SAVECFG_GAME,
	MA_OPT_RMCFG_GAME,
	MA_CTRL_PLAYER1,
	MA_CTRL_EMU,
} menu_id;

me_bind_action me_ctrl_actions[] =
{
	{ "UP       ",  1 << RETRO_DEVICE_ID_JOYPAD_UP},
	{ "DOWN     ",  1 << RETRO_DEVICE_ID_JOYPAD_DOWN },
	{ "LEFT     ",  1 << RETRO_DEVICE_ID_JOYPAD_LEFT },
	{ "RIGHT    ",  1 << RETRO_DEVICE_ID_JOYPAD_RIGHT },
	{ "A BUTTON ",  1 << RETRO_DEVICE_ID_JOYPAD_A },
	{ "B BUTTON ",  1 << RETRO_DEVICE_ID_JOYPAD_B },
	{ "X BUTTON ",  1 << RETRO_DEVICE_ID_JOYPAD_X },
	{ "Y BUTTON ",  1 << RETRO_DEVICE_ID_JOYPAD_Y },
	{ "START    ",  1 << RETRO_DEVICE_ID_JOYPAD_START },
	{ "SELECT   ",  1 << RETRO_DEVICE_ID_JOYPAD_SELECT },
	{ "L BUTTON ",  1 << RETRO_DEVICE_ID_JOYPAD_L },
	{ "R BUTTON ",  1 << RETRO_DEVICE_ID_JOYPAD_R },
	{ "L2 BUTTON ", 1 << RETRO_DEVICE_ID_JOYPAD_L2 },
	{ "R2 BUTTON ", 1 << RETRO_DEVICE_ID_JOYPAD_R2 },
	{ "L3 BUTTON ", 1 << RETRO_DEVICE_ID_JOYPAD_L3 },
	{ "R3 BUTTON ", 1 << RETRO_DEVICE_ID_JOYPAD_R3 },
	{ NULL,       0 }
};

/* Must be a superset of all possible actions. This is used when
 * saving config, and if an entry isn't here, the saver won't see
 * it. */
me_bind_action emuctrl_actions[] =
{
	{ "Save State       ", 1 << EACTION_SAVE_STATE },
	{ "Load State       ", 1 << EACTION_LOAD_STATE },
	{ "Toggle FPS/CPU%  ", 1 << EACTION_TOGGLE_HUD },
	{ "Toggle FF        ", 1 << EACTION_TOGGLE_FF },
	{ "Take Screenshot  ", 1 << EACTION_SCREENSHOT },
	{ NULL,                0 }
};

static int emu_check_save_file(int slot, int *time)
{
	return state_exists(slot);
}

static int emu_save_load_game(int load, int unused)
{
	int ret;

	if (load)
		ret = state_read();
	else
		ret = state_write();

	return ret;
}

// RGB565
static unsigned short fname2color(const char *fname)
{
	return 0xFFFF;
}

#include "libpicofe/menu.c"

static void draw_menu_message(const char *msg, void (*draw_more)(void))  __attribute__((unused));

static const char *mgn_saveloadcfg(int id, int *offs)
{
	return "";
}

static int mh_restore_defaults(int id, int keys)
{
	set_defaults();
	menu_update_msg("defaults restored");
	return 1;
}

static int mh_savecfg(int id, int keys)
{
	if (save_config(id == MA_OPT_SAVECFG_GAME ? CONFIG_TYPE_GAME : CONFIG_TYPE_CORE) == 0)
		menu_update_msg("config saved");
	else
		menu_update_msg("failed to write config");

	return 1;
}

static int mh_rmcfg(int id, int keys)
{
	if (remove_config(id == MA_OPT_RMCFG_GAME ? 1 : 0) == 0)
		menu_update_msg("config removed");
	else
		menu_update_msg("failed to remove config");

	return 1;
}

static void draw_src_bg(void) {
	memcpy(g_menubg_ptr, g_menubg_src_ptr, g_menubg_src_h * g_menubg_src_pp * sizeof(uint16_t));
	menu_darken_bg(g_menubg_ptr, g_menubg_src_ptr, g_menubg_src_h * g_menubg_src_pp, 0);
}

static int mh_set_core(int id, int keys) {
	if (corelist && id < corelist_len)
		snprintf(core_path, sizeof(core_path), "%s/%s", cores_path, corelist[id]->d_name);

	return 1;
}

static int core_selector(const struct dirent *ent) {
	return has_suffix_i(ent->d_name, "_libretro.so");
}

static int menu_loop_core_page(int offset, int keys) {
	static int sel = 0;
	menu_entry e_menu_cores[MENU_ITEMS_PER_PAGE + 2] = {0}; /* +2 for Next, NULL */
	size_t menu_idx = 0;
	int i;
	char names[MENU_ITEMS_PER_PAGE][MAX_PATH];

	for (i = offset, menu_idx = 0; i < corelist_len && menu_idx < MENU_ITEMS_PER_PAGE; i++) {
		menu_entry *option;
		struct dirent *ent = corelist[i];
		option = &e_menu_cores[menu_idx];
		core_extract_name(ent->d_name, names[menu_idx], sizeof(names[menu_idx]));

		option->name = names[menu_idx];
		option->beh = MB_OPT_CUSTOM;
		option->id = i;
		option->enabled = 1;
		option->selectable = 1;
		option->handler = mh_set_core;
		menu_idx++;
	}

	if (i < corelist_len) {
		menu_entry *option;
		option = &e_menu_cores[menu_idx];
		option->name = "Next page";
		option->beh = MB_OPT_CUSTOM;
		option->id = i;
		option->enabled = 1;
		option->selectable = 1;
		option->handler = menu_loop_core_page;
	}

	return me_loop(e_menu_cores, &sel);
}

int menu_select_core(void) {
	int ret = -1;
	getcwd(cores_path, MAX_PATH);

	corelist_len = scandir(cores_path, &corelist, core_selector, alphasort);
	if (!corelist_len) return -1;

	plat_video_menu_enter(1);

	if (menu_loop_core_page(0, 0) < 0)
		goto finish;

	if (core_path[0] == '\0')
		goto finish;

	ret = 0;
finish:
	/* wait until menu, ok, back is released */
	while (in_menu_wait_any(NULL, 50) & (PBTN_MENU|PBTN_MOK|PBTN_MBACK))
		;

	plat_video_menu_leave();

	if (corelist_len > 0) {
		while (corelist_len--)
			free(corelist[corelist_len]);
		free(corelist);
		corelist = NULL;
	}
	return ret;
}

int hidden_file_filter(struct dirent **namelist, int count, const char *basedir) {
	int newcount = 0;

	for (int i = 0; i < count; i++) {
		if (namelist[i]->d_name[0] == '.' && namelist[i]->d_name[1] != '.') {
			free(namelist[i]);
			namelist[i] = NULL;
		}
	}

	for (int i = 0; i < count; i++) {
		if (namelist[i] != NULL)
			namelist[newcount++] = namelist[i];
	}

	return newcount;
}

const char *select_content(void) {
	const char *fname = NULL;
	char content_path[MAX_PATH];
	const char **extensions = core_extensions();
	const char **exts_with_zip = NULL;
	int i = 0, size = 0;

	if (content && strlen(content->path)) {
		strncpy(content_path, content->path, sizeof(content_path) - 1);
	} else {
		core_load_last_opened(content_path, sizeof(content_path));
	}

	if (!content_path[0]) {
		if (getenv("CONTENT_DIR")) {
			strncpy(content_path, getenv("CONTENT_DIR"), sizeof(content_path) - 1);
#ifdef CONTENT_DIR
		} else {
			strncpy(content_path, CONTENT_DIR, sizeof(content_path) - 1);
#else
		} else if (getenv("HOME")) {
			strncpy(content_path, getenv("HOME"), sizeof(content_path) - 1);
#endif
		}
	}

	content_path[sizeof(content_path) - 1] = '\0';

	if (extensions) {
		for (size = 0; extensions[size]; size++)
			;
	}

	exts_with_zip = calloc(size + 2, sizeof (char *)); /* add 2 for "zip", NULL */

	if (exts_with_zip) {
		for (i = 0; extensions[i]; i++) {
			exts_with_zip[i] = extensions[i];
		}
		exts_with_zip[i] = "zip";
	} else {
		exts_with_zip = extensions;
	}

	fname = menu_loop_romsel(content_path, sizeof(content_path), exts_with_zip, hidden_file_filter);

	if (exts_with_zip != extensions)
		free(exts_with_zip);

	return fname;
}

int menu_select_content(char *filename, size_t len) {
	const char *fname = NULL;
	int ret = -1;

	plat_video_menu_enter(1);
	fname = select_content();
	if (!fname)
		goto finish;

	strncpy(filename, fname, len - 1);
	if (g_autostateld_opt)
		resume_slot = 0;
	ret = 0;

finish:
        /* wait until menu, ok, back is released */
	while (in_menu_wait_any(NULL, 50) & (PBTN_MENU|PBTN_MOK|PBTN_MBACK))
		;

	plat_video_menu_leave();
	return ret;
}

static int menu_loop_select_content(int id, int keys) {
	const char *fname = select_content();

	if (fname == NULL)
		return -1;

	new_fname = fname;

	return 1;
}

static void load_new_content(const char *fname) {
	const struct core_override *override = get_overrides();

	if (!override || override->needs_reopen) {
#ifdef FUNKEY_S
		FK_LoadNewGame(fname);
		/* Does not return */
#else
		core_close();
		core_open(core_path);
#endif
	} else {
#ifdef FUNKEY_S
		FK_Autosave();
#endif
		core_unload();
	}
	core_load();

	content = content_init(fname);
	if (!content) {
		PA_ERROR("Couldn't allocate memory for content\n");
		quit(-1);
	}

	set_defaults();

	if (core_load_content(content)) {
		quit(-1);
	}

	load_config();
	load_config_keys();

	if (g_autostateld_opt) {
		resume_slot = 0;
		state_resume();
	}

#ifdef FUNKEY_S
	FK_Resume();
#endif
}

static int menu_loop_disc(int id, int keys)
{
	static int sel = 0;
	menu_entry e_menu_disc_options[2] = {0};
	unsigned disc = disc_get_index() + 1;
	menu_entry *option = &e_menu_disc_options[0];

	option->name = "Disc";
	option->beh = MB_OPT_RANGE;
	option->var = &disc;
	option->min = 1;
	option->max = disc_get_count();
	option->enabled = 1;
	option->need_to_save = 1;
	option->selectable = 1;

	me_loop(e_menu_disc_options, &sel);

	if (disc_get_index() + 1 != disc)
		disc_switch_index(disc - 1);

	return 0;
}

static int menu_loop_cheats_page(int offset, int keys) {
	static int sel = 0;
	menu_entry *e_menu_cheats;
	size_t i, menu_idx;

	/* cheats + 2 for possible "Next page" +  NULL */
	e_menu_cheats = (menu_entry *)calloc(cheats->count + 2, sizeof(menu_entry));

	if (!e_menu_cheats) {
		PA_ERROR("Error allocating cheats\n");
		return 0;
	}

	for (i = offset, menu_idx = 0; i < cheats->count && menu_idx < MENU_ITEMS_PER_PAGE; i++) {
		struct cheat *cheat = &cheats->cheats[i];
		menu_entry *option;

		option = &e_menu_cheats[menu_idx];

		option->name = cheat->name;
		option->beh = MB_OPT_ONOFF;
		option->var = &cheat->enabled;
		option->enabled = 1;
		option->mask = 1;
		option->need_to_save = 1;
		option->selectable = 1;
		option->help = cheat->info;
		menu_idx++;
	}

	if (i < cheats->count) {
		menu_entry *option;
		option = &e_menu_cheats[menu_idx];
		option->name = "Next page";
		option->beh = MB_OPT_CUSTOM;
		option->id = i;
		option->enabled = 1;
		option->selectable = 1;
		option->handler = menu_loop_cheats_page;
	}

	me_loop(e_menu_cheats, &sel);
	free(e_menu_cheats);

	return 0;
}

static int menu_loop_cheats(int id, int keys)
{
	int ret = menu_loop_cheats_page(0, keys);
	core_apply_cheats(cheats);
	return ret;
}

static int menu_loop_core_options_page(int offset, int keys) {
	static int sel = 0;
	menu_entry *e_menu_core_options;
	size_t i, menu_idx;

	/* core_option + 2 for possible "Next page" +  NULL */
	e_menu_core_options = (menu_entry *)calloc(core_options.visible_len + 2, sizeof(menu_entry));

	if (!e_menu_core_options) {
		PA_ERROR("Error allocating core options\n");
		return 0;
	}

	for (i = offset, menu_idx = 0; i < core_options.len && menu_idx < MENU_ITEMS_PER_PAGE; i++) {
		struct core_option_entry *entry = &core_options.entries[i];
		menu_entry *option;
		const char *key = entry->key;

		if (entry->blocked || !entry->visible)
			continue;

		option = &e_menu_core_options[menu_idx];

		option->name = entry->desc;
		option->beh = MB_OPT_ENUM;
		option->var = options_get_value_ptr(key);
		option->enabled = 1;
		option->need_to_save = 1;
		option->selectable = 1;
		option->data = options_get_options(key);
		option->help = entry->info;
		menu_idx++;
	}

	for (; i < core_options.len; i++) {
		struct core_option_entry *entry = &core_options.entries[i];
		if (!entry->blocked && entry->visible)
			break;
	}

	if (i < core_options.len) {
		menu_entry *option;
		option = &e_menu_core_options[menu_idx];
		option->name = "Next page";
		option->beh = MB_OPT_CUSTOM;
		option->id = i;
		option->enabled = 1;
		option->selectable = 1;
		option->handler = menu_loop_core_options_page;
	}

	me_loop(e_menu_core_options, &sel);

	options_update_changed();

	free(e_menu_core_options);

	return 0;
}

static int menu_loop_core_options(int id, int keys)
{
	return menu_loop_core_options_page(0, keys);
}

static const char h_rm_config_game[]  = "Removes game-specific config file";

static const char h_restore_def[]     = "Switches back to default settings";

static const char h_show_fps[]        = "Shows frames and vsyncs per second";
static const char h_show_cpu[]        = "Shows CPU usage";

#if (SCREEN_WIDTH >= 320)
static const char h_enable_drc[]      = "Dynamically adjusts audio rate for smoother video";

static const char h_audio_buffer_size[]        =
	"The size of the audio buffer, in frames. Higher\n"
	"values reduce the risk of audio crackling at the\n"
	"cost of delayed sound.";

static const char h_scale_size[]        =
	"How much to stretch the screen when scaling. Native\n"
	"does no stretching. Aspect uses the correct aspect\n"
	"ratio. Full uses the whole screen.";

static const char h_scale_filter[]        =
	"When stretching, how missing pixels are filled.\n"
	"Nearest copies the last pixel. Sharp keeps pixels\n"
	"aligned where possible. Smooth adds a blur effect.";
	
static const char h_use_srm[]        =
	"Use .srm files for SRAM saves, needed for\n"
	"compatibility with mainline retroarch saves.\n"
	"Save file compression needs to be off in retroarch.";

static const char *men_scale_size[] = { "Native", "Aspect", "Full", NULL};
#else
static const char h_enable_drc[]      =
	"Dynamically adjusts audio rate for\n"
	"smoother video.";

static const char h_audio_buffer_size[]        =
	"The audio buffer size, in frames.\n"
  "Higher values reduce the risk of audio\n"
	"crackling at the cost of delayed sound.";

static const char h_scale_size[]        =
	"How to fill the display. Native does\n"
	"no stretching. Aspect keeps the correct\n"
	"aspect ratio. Full uses the whole\n"
	"screen. Crop hides pixels on the sides.";

static const char h_scale_filter[]        =
	"When stretching, how missing pixels\n"
	"are filled. Nearest copies the last\n"
	"pixel. Sharp tries to keep pixels\n"
	"aligned. Smooth adds a blur effect.";
	
static const char h_use_srm[]        =
	"Use .srm files for SRAM saves,\n"
	"needed for compatibility with mainline\n"
	"retroarch saves. Save file compression\n"
	"needs to be off in retroarch.";

static const char *men_scale_size[] = { "Native", "Aspect", "Full", "Crop", NULL};
#endif

static const char *men_scale_filter[] = { "Nearest", "Sharp", "Smooth", NULL};

static menu_entry e_menu_video_options[] =
{
	mee_onoff_h      ("Show FPS",                 0, show_fps, 1, h_show_fps),
	mee_onoff_h      ("Show CPU %%",              0, show_cpu, 1, h_show_cpu),
	mee_enum_h       ("Screen size",              0, scale_size, men_scale_size, h_scale_size),
	mee_enum_h       ("Filter",                   0, scale_filter, men_scale_filter, h_scale_filter),
	mee_range_h      ("Audio buffer",             0, audio_buffer_size, 1, 15, h_audio_buffer_size),
	mee_onoff_h      ("Audio adjustment",         0, enable_drc, 1, h_enable_drc),
	mee_end,
};

static int menu_loop_video_options(int id, int keys)
{
	static int sel = 0;

	me_loop(e_menu_video_options, &sel);
	plat_reinit();

	return 0;
}

static int key_config_loop_wrap(int id, int keys)
{
	const struct core_override *override = get_overrides();
	me_bind_action *actions = CORE_OVERRIDE(override, actions, me_ctrl_actions);
	size_t action_size = CORE_OVERRIDE(override, action_size, array_size(me_ctrl_actions));
	me_bind_action *emu_actions = CORE_OVERRIDE(override, emu_actions, emuctrl_actions);
	size_t emu_action_size = CORE_OVERRIDE(override, emu_action_size, array_size(emuctrl_actions));

	switch (id) {
	case MA_CTRL_PLAYER1:
		key_config_loop(actions, action_size - 1, 0);
		break;
	case MA_CTRL_EMU:
		key_config_loop(emu_actions, emu_action_size - 1, -1);
		break;
	default:
		break;
	}
	return 0;
}

const char *config_label(int id, int *offs) {
	return config_override ? "Loaded: game config" : "Loaded: global config";
}

static menu_entry e_menu_config_options[] =
{
	mee_onoff_h      ("Use .srm saves",           0, use_srm, 1, h_use_srm),
	mee_label        (""),
	mee_cust_nosave  ("Save global config",       MA_OPT_SAVECFG,      mh_savecfg, mgn_saveloadcfg),
	mee_cust_nosave  ("Save game config",         MA_OPT_SAVECFG_GAME, mh_savecfg, mgn_saveloadcfg),
	mee_handler_id_h ("Delete game config",       MA_OPT_RMCFG_GAME,   mh_rmcfg,   h_rm_config_game),
	mee_handler_h    ("Restore defaults",         mh_restore_defaults, h_restore_def),
	mee_label        (""),
	mee_label_mk     (0,                          config_label),
	mee_end,
};

static int menu_loop_config_options(int id, int keys)
{
	static int sel = 0;
	me_enable(e_menu_config_options, MA_OPT_RMCFG_GAME, config_override == 1);

	me_loop(e_menu_config_options, &sel);

	return 0;
}

static menu_entry e_menu_options[] =
{
	mee_handler   ("Audio and video",    menu_loop_video_options),
	mee_handler_id("Emulator options",   MA_OPT_CORE_OPTS,    menu_loop_core_options),
	mee_handler_id("Player controls",    MA_CTRL_PLAYER1,     key_config_loop_wrap),
	mee_handler_id("Emulator controls",  MA_CTRL_EMU,         key_config_loop_wrap),
	mee_handler   ("Save config",        menu_loop_config_options),
	mee_end,
};

static int menu_loop_options(int id, int keys)
{
	static int sel = 0;
	me_loop(e_menu_options, &sel);

	return 0;
}

static int main_menu_handler(int id, int keys)
{
	switch (id)
	{
	case MA_MAIN_RESUME_GAME:
		return 1;
	case MA_MAIN_SAVE_STATE:
		return menu_loop_savestate(0);
	case MA_MAIN_LOAD_STATE:
		return menu_loop_savestate(1);
	case MA_MAIN_RESET_GAME:
		current_core.retro_reset();
		return 1;
	case MA_MAIN_EXIT:
		should_quit = 1;
		return 1;
	default:
		lprintf("%s: something unknown selected\n", __FUNCTION__);
		break;
	}

	return 0;
}

static menu_entry e_menu_main[] =
{
	mee_handler_id("Resume game",        MA_MAIN_RESUME_GAME, main_menu_handler),
	mee_handler_id("Save state",         MA_MAIN_SAVE_STATE,  main_menu_handler),
	mee_handler_id("Load state",         MA_MAIN_LOAD_STATE,  main_menu_handler),
	mee_handler_id("Disc control",       MA_MAIN_DISC_CTRL,   menu_loop_disc),
	mee_handler_id("Cheats",             MA_MAIN_CHEATS,      menu_loop_cheats),
	mee_handler   ("Options",            menu_loop_options),
	mee_handler_id("Reset game",         MA_MAIN_RESET_GAME,  main_menu_handler),
	mee_handler_id("Load new game",      MA_MAIN_CONTENT_SEL, menu_loop_select_content),
	mee_handler_id("Exit",               MA_MAIN_EXIT,        main_menu_handler),
	mee_end,
};

static void draw_savestate_bg(int slot)
{
	char filename[MAX_PATH];
	int w, h, bpp;
	size_t bufsize = SCREEN_PITCH * SCREEN_HEIGHT;
	void *buf = calloc(bufsize, sizeof(char));

	if (!buf) {
		PA_WARN("Couldn't allocate savestate background");
		goto finish;
	}
	state_file_name(filename, MAX_PATH, slot);

	if (plat_load_screen(filename, buf, bufsize, &w, &h, &bpp))
		goto finish;

	if (bpp == sizeof(uint16_t)) {
		menu_darken_bg(g_menubg_ptr, buf, w * h, 0);
		drew_alt_bg = 1;
	}

finish:
	if (buf)
		free(buf);
}

void menu_begin(void)
{
	if (!drew_alt_bg)
		draw_src_bg();
}

void menu_end(void)
{
	drew_alt_bg = 0;
}

void menu_loop(void)
{
	static int sel = 0;
	bool needs_disc_ctrl = disc_get_count() > 1;

	plat_video_menu_enter(1);

	me_enable(e_menu_options, MA_OPT_CORE_OPTS, core_options.visible_len > 0);

	me_enable(e_menu_main, MA_MAIN_SAVE_STATE, state_allowed());
	me_enable(e_menu_main, MA_MAIN_LOAD_STATE, state_allowed());
	me_enable(e_menu_main, MA_MAIN_CHEATS, cheats != NULL);

	me_enable(e_menu_main, MA_MAIN_DISC_CTRL, needs_disc_ctrl);

#ifdef MMENU
	if (state_allowed()) {
		me_enable(e_menu_main, MA_MAIN_SAVE_STATE, mmenu == NULL);
		me_enable(e_menu_main, MA_MAIN_LOAD_STATE, mmenu == NULL);
	}
#endif
	me_loop(e_menu_main, &sel);

	/* wait until menu, ok, back is released */
	while (in_menu_wait_any(NULL, 50) & (PBTN_MENU|PBTN_MOK|PBTN_MBACK))
		;

	if (new_fname) {
		load_new_content(new_fname);
		new_fname = NULL;
	}

	/* Force the hud to clear */
	plat_video_set_msg(NULL, 0, 0);
	plat_video_menu_leave();
}

int menu_init(void)
{
	menu_init_base();

	g_menubg_src_ptr = calloc(g_menubg_src_pp * g_menubg_src_h, sizeof(uint16_t));
	g_menubg_ptr = calloc(g_menuscreen_w * g_menuscreen_pp, sizeof(uint16_t));
	if (g_menubg_src_ptr == NULL || g_menubg_ptr == NULL) {
		fprintf(stderr, "OOM\n");
		return -1;
	}
	return 0;
}

void menu_finish(void)
{
	if (g_menubg_src_ptr) {
		free(g_menubg_src_ptr);
		g_menubg_src_ptr = NULL;
	}

	if (g_menubg_ptr) {
		free(g_menubg_ptr);
		g_menubg_ptr = NULL;
	}
}

static void debug_menu_loop(void)
{
}

void menu_update_msg(const char *msg)
{
	strncpy(menu_error_msg, msg, sizeof(menu_error_msg));
	menu_error_msg[sizeof(menu_error_msg) - 1] = 0;

	menu_error_time = plat_get_ticks_ms();
	PA_INFO("%s\n", menu_error_msg);
}
