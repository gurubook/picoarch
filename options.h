#ifndef _OPTIONS_H__
#define _OPTIONS_H__
#include "libretro.h"
#include "scale.h"

extern int show_fps;
extern int show_cpu;
extern int show_hud;
extern int limit_frames;
extern int enable_audio;
extern int enable_drc;
extern int use_srm;
extern unsigned audio_buffer_size;
extern enum scale_size scale_size;
extern enum scale_filter scale_filter;

struct core_option_entry {
	char *key;
	char *desc;
	char *info;
	int value;
	int prev_value;
	int default_value;
	bool blocked;
	bool visible;
	char **values;
	char **labels;
	char *retro_var_value;
};

struct core_options {
	size_t len;
	size_t visible_len;
	bool changed;
	struct core_option_entry *entries;
};

extern struct core_options core_options;

void options_init(const struct retro_core_option_definition *defs);
void options_init_variables(const struct retro_variable *vars);
bool options_changed(void);
void options_update_changed(void);
const char* options_get_key(int index);

struct core_option_entry* options_get_entry(const char* key);

const char* options_get_value(const char* key);
int* options_get_value_ptr(const char* key);
int options_get_value_index(const char* key);

void options_set_value(const char* key, const char *value);
void options_set_value_index(const char* key, int value);
void options_set_visible(const char* key, bool visible);

const char** options_get_options(const char* key);
void options_free(void);

#endif
