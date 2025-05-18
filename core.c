#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "cheat.h"
#include "core.h"
#include "libpicofe/input.h"
#include "main.h"
#include "options.h"
#include "overrides.h"
#include "plat.h"
#include "util.h"
#include "video.h"

struct core_cbs current_core;
char core_path[MAX_PATH];
struct content *content;
static struct string_list *extensions;
struct cheats *cheats;

double sample_rate;
double frame_rate;
double aspect_ratio;
unsigned audio_buffer_size_override;
int state_slot;

static char config_dir[MAX_PATH];
static char save_dir[MAX_PATH];
static char system_dir[MAX_PATH];
static struct retro_disk_control_ext_callback disk_control_ext;

static uint32_t buttons = 0;
static int polled = 0;

static int core_load_game_info(struct content *content, struct retro_game_info *game_info) {
	struct retro_system_info info = {};
	current_core.retro_get_system_info(&info);

	return content_load_game_info(content, game_info, info.need_fullpath);
}

void config_file_name(char *buf, size_t len, config_type config_type)
{
	if (config_type == CONFIG_TYPE_AUTO) {
		snprintf(buf, len, "%s%s", config_dir, "picoarch-auto.cfg");
	} else if (config_type == CONFIG_TYPE_GAME && content) {
		content_based_name(content, buf, len, save_dir, NULL, ".cfg");
	} else {
		snprintf(buf, len, "%s%s", config_dir, "picoarch.cfg");
	}
}

void save_relative_path(char *buf, size_t len, const char *basename) {
	snprintf(buf, len, "%s%s", save_dir, basename);
}

void sram_write(void) {
	char filename[MAX_PATH];
	FILE *sram_file = NULL;
	void *sram;

	size_t sram_size = current_core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
	if (!sram_size) {
		return;
	}
	
	if (use_srm == 1) {
		content_based_name(content, filename, MAX_PATH, save_dir, NULL, ".srm");
	} else {
		content_based_name(content, filename, MAX_PATH, save_dir, NULL, ".sav");
	}

	sram_file = fopen(filename, "w");
	if (!sram_file) {
		PA_ERROR("Error opening SRAM file: %s\n", strerror(errno));
		return;
	}

	sram = current_core.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);

	if (!sram || sram_size != fwrite(sram, 1, sram_size, sram_file)) {
		PA_ERROR("Error writing SRAM data to file\n");
	}

	fclose(sram_file);

	sync();
}

void sram_read(void) {
	char filename[MAX_PATH];
	FILE *sram_file = NULL;
	void *sram;

	size_t sram_size = current_core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
	if (!sram_size) {
		return;
	}

	if (use_srm == 1) {
		content_based_name(content, filename, MAX_PATH, save_dir, NULL, ".srm");
		
		sram_file = fopen(filename, "r");
		if (!sram_file) {
			memset(filename, 0, sizeof(filename));
			content_based_name(content, filename, MAX_PATH, save_dir, NULL, ".sav");
		}
	} else {
		content_based_name(content, filename, MAX_PATH, save_dir, NULL, ".sav");
		
		sram_file = fopen(filename, "r");
		if (!sram_file) {
			memset(filename, 0, sizeof(filename));
			content_based_name(content, filename, MAX_PATH, save_dir, NULL, ".srm");
		}
	}

	sram_file = fopen(filename, "r");
	if (!sram_file) {
		return;
	}

	sram = current_core.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);

	if (!sram || !fread(sram, 1, sram_size, sram_file)) {
		PA_ERROR("Error reading SRAM data\n");
	}

	fclose(sram_file);
}

void rtc_write(void) {
	char filename[MAX_PATH];
	FILE *rtc_file = NULL;
	void *rtc;

	size_t rtc_size = current_core.retro_get_memory_size(RETRO_MEMORY_RTC);
	if (!rtc_size) {
		return;
	}

	content_based_name(content, filename, MAX_PATH, save_dir, NULL, ".rtc");

	rtc_file = fopen(filename, "w");
	if (!rtc_file) {
		PA_ERROR("Error opening RTC file: %s\n", strerror(errno));
		return;
	}

	rtc = current_core.retro_get_memory_data(RETRO_MEMORY_RTC);

	if (!rtc || rtc_size != fwrite(rtc, 1, rtc_size, rtc_file)) {
		PA_ERROR("Error writing RTC data to file\n");
	}

	fclose(rtc_file);

	sync();
}

void rtc_read(void) {
	char filename[MAX_PATH];
	FILE *rtc_file = NULL;
	void *rtc;

	size_t rtc_size = current_core.retro_get_memory_size(RETRO_MEMORY_RTC);
	if (!rtc_size) {
		return;
	}

	content_based_name(content, filename, MAX_PATH, save_dir, NULL, ".rtc");

	rtc_file = fopen(filename, "r");
	if (!rtc_file) {
		return;
	}

	rtc = current_core.retro_get_memory_data(RETRO_MEMORY_RTC);

	if (!rtc || !fread(rtc, 1, rtc_size, rtc_file)) {
		PA_ERROR("Error reading RTC data\n");
	}

	fclose(rtc_file);
}

bool state_allowed(void) {
	return current_core.retro_serialize_size() > 0;
}

void state_file_name(char *name, size_t size, int slot) {
	char extension[6] = {0};

	snprintf(extension, 6, ".st%d", slot);
	content_based_name(content, name, MAX_PATH, save_dir, NULL, extension);
}

bool state_exists(int slot) {
	char fname[MAX_PATH];
	state_file_name(fname, sizeof(fname), slot);
	return access(fname, F_OK) == 0;
}

int state_read(void) {
	char filename[MAX_PATH];
	struct stat stat;
	size_t state_size;
	FILE *state_file = NULL;
	void *state = NULL;
	int ret = -1;

	state_file_name(filename, MAX_PATH, state_slot);

	state_file = fopen(filename, "r");
	if (!state_file) {
		PA_ERROR("Error opening state file: %s\n", strerror(errno));
		goto error;
	}

	if (fstat(fileno(state_file), &stat) == -1) {
		PA_ERROR("Couldn't read state file size: %s\n", strerror(errno));
		goto error;
	}

	state_size = stat.st_size;

	state = calloc(1, state_size);
	if (!state) {
		PA_ERROR("Couldn't allocate memory for state\n");
		goto error;
	}

	if (state_size != fread(state, 1, state_size, state_file)) {
		PA_ERROR("Error reading state data from file\n");
		goto error;
	}

	if (!current_core.retro_unserialize(state, state_size)) {
		PA_ERROR("Error restoring save state\n", strerror(errno));
		goto error;
	}

	ret = 0;
error:
	if (state)
		free(state);
	if (state_file)
		fclose(state_file);
	return ret;
}

int state_write(void) {
	char filename[MAX_PATH];
	FILE *state_file = NULL;
	void *state = NULL;
	int ret = -1;

	size_t state_size = current_core.retro_serialize_size();
	if (!state_size) {
		return false;
	}

	state = calloc(1, state_size);
	if (!state) {
		PA_ERROR("Couldn't allocate memory for state\n");
		goto error;
	}

	state_file_name(filename, MAX_PATH, state_slot);

	state_file = fopen(filename, "w");
	if (!state_file) {
		PA_ERROR("Error opening state file: %s\n", strerror(errno));
		goto error;
	}

	if (!current_core.retro_serialize(state, state_size)) {
		PA_ERROR("Error creating save state\n", strerror(errno));
		goto error;
	}

	if (state_size != fwrite(state, 1, state_size, state_file)) {
		PA_ERROR("Error writing state data to file\n");
		goto error;
	}

	plat_dump_screen(filename);

	ret = 0;
error:
	if (state)
		free(state);
	if (state_file)
		fclose(state_file);

	sync();
	return ret;
}

unsigned disc_get_count(void) {
	if (disk_control_ext.get_num_images)
		return disk_control_ext.get_num_images();

	return 0;
}

unsigned disc_get_index(void) {
	if (disk_control_ext.get_image_index)
		return disk_control_ext.get_image_index();

	return 0;
}

bool disc_switch_index(unsigned index) {
	bool ret = false;
	if (!disk_control_ext.set_eject_state || !disk_control_ext.set_image_index)
		return false;

	disk_control_ext.set_eject_state(true);
	ret = disk_control_ext.set_image_index(index);
	disk_control_ext.set_eject_state(false);

	return ret;
}

bool disc_replace_index(unsigned index, const char *content_path) {
	bool ret = false;
	struct retro_game_info info = {};
	struct content *content;
	if (!disk_control_ext.replace_image_index)
		return false;

	content = content_init(content_path);
	if (!content)
		goto finish;

	if (core_load_game_info(content, &info))
		goto finish;

	ret = disk_control_ext.replace_image_index(index, &info);

finish:
	content_free(content);
	return ret;
}

static void set_directories(const char *core_name) {
	const char *home = getenv("HOME");
	char *dst = save_dir;
	int len = MAX_PATH;
#ifndef MINUI
	char cwd[MAX_PATH];
#endif

	char picoarch_root[MAX_PATH] = "";
	DIR* dir;

	/* Example:
	 * Old save / config dir: ~/.picoarch-gambatte
	 * New save / config dir: ~/.picoarch/data/gambatte
	 * System dir: ./system
	 *
	 * FunKey S:
	 * System dir: ~/.picoarch/system
	 *
	 * MinUI:
	 * Save / config dir: ~/.picoarch-gambatte
	 * System dir: ~/.picoarch-gambatte
	 */
	if (home != NULL) {
		snprintf(dst, len, "%s/.picoarch-%s/", home, core_name);
#ifdef MINUI
		mkdir(dst, 0755);
		strncpy(config_dir, save_dir, MAX_PATH-1);
		strncpy(system_dir, save_dir, MAX_PATH-1);
#else
		snprintf(picoarch_root, MAX_PATH, "%s/.picoarch", home);
		mkdir(picoarch_root, 0755);

		dir = opendir(dst);
		if (dir) {
			/* Use old save dir if exists */
			closedir(dir);
		} else {
			snprintf(dst, len, "%s/data", picoarch_root);
			mkdir(dst, 0755);
			snprintf(dst, len, "%s/data/%s/", picoarch_root, core_name);
			mkdir(dst, 0755);
		}
	}

	strncpy(config_dir, save_dir, MAX_PATH-1);

#ifdef FUNKEY_S
	if (strlen(picoarch_root)) {
		snprintf(system_dir, MAX_PATH, "%s/system", picoarch_root);
		mkdir(system_dir, 0755);
	} else
#endif  /* FUNKEY_S */
	if (getcwd(cwd, MAX_PATH)) {
		snprintf(system_dir, MAX_PATH, "%s/system", cwd);
		mkdir(system_dir, 0755);
	} else {
		PA_FATAL("Can't find system directory\n");
	}
#endif  /* MINUI */
	PA_INFO("Config dir: %s\n", config_dir);
	PA_INFO("Save dir: %s\n", save_dir);
	PA_INFO("System dir: %s\n", system_dir);
}

static bool pa_environment(unsigned cmd, void *data) {
	switch(cmd) {
	case RETRO_ENVIRONMENT_GET_OVERSCAN: { /* 2 */
		bool *out = (bool *)data;
		if (out)
			*out = true;
		break;
	}
	case RETRO_ENVIRONMENT_GET_CAN_DUPE: { /* 3 */
		bool *out = (bool *)data;
		if (out)
			*out = true;
		break;
	}
	case RETRO_ENVIRONMENT_SET_MESSAGE: { /* 6 */
		const struct retro_message *message = (const struct retro_message*)data;
		if (message) {
			PA_INFO("%s\n", message->msg);
		}

		break;
	}
	case RETRO_ENVIRONMENT_SHUTDOWN: { /* 7 */
		should_quit = 1;

		break;
	}
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: { /* 9 */
		const char **out = (const char **)data;
		if (out)
			*out = system_dir;
		break;
	}
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: { /* 10 */
		const enum retro_pixel_format format = *(enum retro_pixel_format *)data;

		if (format == RETRO_PIXEL_FORMAT_RGB565 ||
		    format == RETRO_PIXEL_FORMAT_XRGB8888) {
			video_set_pixel_format(format);
		} else {
			return false;
		}

		break;
	}
	case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE: { /* 13 */
		const struct retro_disk_control_callback *var =
			(const struct retro_disk_control_callback *)data;

		if (var) {
			memset(&disk_control_ext, 0, sizeof(struct retro_disk_control_ext_callback));
			memcpy(&disk_control_ext, var, sizeof(struct retro_disk_control_callback));
		}
		break;
	}
	case RETRO_ENVIRONMENT_GET_VARIABLE: { /* 15 */
		struct retro_variable *var = (struct retro_variable *)data;
		if (var && var->key) {
			var->value = options_get_value(var->key);
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_VARIABLES: { /* 16 */
		const struct retro_variable *vars = (const struct retro_variable *)data;
		options_free();
		if (vars) {
			options_init_variables(vars);
			load_config();
		}
		break;
	}
	case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: { /* 17 */
		bool *out = (bool *)data;
		if (out)
			*out = options_changed();
		break;
	}
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: { /* 27 */
		struct retro_log_callback *log_cb = (struct retro_log_callback *)data;
		if (log_cb)
			log_cb->log = pa_log;
		break;
	}
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: { /* 31 */
		const char **out = (const char **)data;
		if (out)
			*out = save_dir;
		break;
	}
	case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: { /* 52 */
		bool *out = (bool *)data;
		if (out)
			*out = true;
		break;
	}
	case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: { /* 52 */
		unsigned *out = (unsigned *)data;
		if (out)
			*out = 1;
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: { /* 53 */
		options_free();
		if (data) {
			options_init((const struct retro_core_option_definition *)data);
			load_config();
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: { /* 54 */
		const struct retro_core_options_intl *options = (const struct retro_core_options_intl *)data;
		if (options && options->us) {
			options_free();
			options_init(options->us);
			load_config();
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: { /* 55 */
		const struct retro_core_option_display *display =
			(const struct retro_core_option_display *)data;

		if (display)
			options_set_visible(display->key, display->visible);
		break;
	}
	case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION: { /* 57 */
		unsigned *out =	(unsigned *)data;
		if (out)
			*out = 1;
		break;
	}
	case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE: { /* 58 */
		const struct retro_disk_control_ext_callback *var =
			(const struct retro_disk_control_ext_callback *)data;

		if (var) {
			memcpy(&disk_control_ext, var, sizeof(struct retro_disk_control_ext_callback));
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_MESSAGE_EXT: { /* 6 */
		const struct retro_message_ext *message = (const struct retro_message_ext*)data;
		if (message) {
			pa_log(message->level, "%s\n", message->msg);
		}

		break;
	}
	case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK: { /* 62 */
		const struct retro_audio_buffer_status_callback *cb =
			(const struct retro_audio_buffer_status_callback *)data;
		if (cb) {
			current_core.retro_audio_buffer_status = cb->callback;
		} else {
			current_core.retro_audio_buffer_status = NULL;
		}
		break;
	}
	case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY: { /* 63 */
		const unsigned *latency_ms = (const unsigned *)data;
		if (latency_ms) {
			unsigned frames = *latency_ms * frame_rate / 1000;
			if (frames < 30)
				audio_buffer_size_override = frames;
			else
				PA_WARN("Audio buffer change out of range (%d), ignored\n", frames);
		}
		break;
	}
	default:
		PA_DEBUG("Unsupported environment cmd: %u\n", cmd);
		return false;
	}

	return true;
}

static void pa_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch) {
	if (data && !should_quit) {
		pa_track_render();
		video_process(data, width, height, pitch);
	}
}

static void pa_audio_sample(int16_t left, int16_t right) {
	const struct audio_frame frame = { .left = left, .right = right };
	if (!should_quit && enable_audio)
		plat_sound_write(&frame, 1);
}

static size_t pa_audio_sample_batch(const int16_t *data, size_t frames) {
	if (!should_quit && enable_audio)
		plat_sound_write((const struct audio_frame *)data, frames);
	return frames;
}

static void pa_input_poll(void) {
	int actions[IN_BINDTYPE_COUNT] = { 0, };
	unsigned int emu_act;
	int which = EACTION_NONE;

	in_update(actions);
	emu_act = actions[IN_BINDTYPE_EMU];
	if (emu_act) {
		for (; !(emu_act & 1); emu_act >>= 1, which++)
			;
		emu_act = which;
	}
	handle_emu_action(which);

	buttons = actions[IN_BINDTYPE_PLAYER12];
	polled = 1;
}

static int16_t pa_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
	if (port == 0 && device == RETRO_DEVICE_JOYPAD && index == 0) {
		if (!polled)
			pa_input_poll();

		if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
			return buttons;

		return (buttons >> id) & 1;
	}

	return 0;
}

void core_extract_name(const char* core_file, char *buf, size_t len) {
	char *suffix = NULL;

	strncpy(buf, basename(core_file), MAX_PATH);
	buf[len - 1] = 0;

	suffix = strrchr(buf, '_');
	if (suffix && !strcmp(suffix, "_libretro.so"))
		*suffix = 0;
	else {
		suffix = strrchr(buf, '.');
		if (suffix && !strcmp(suffix, ".so"))
			*suffix = 0;
	}
}

int core_open(const char *corefile) {
	struct retro_system_info info = {};

	void (*set_environment)(retro_environment_t) = NULL;
	void (*set_video_refresh)(retro_video_refresh_t) = NULL;
	void (*set_audio_sample)(retro_audio_sample_t) = NULL;
	void (*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;
	void (*set_input_poll)(retro_input_poll_t) = NULL;
	void (*set_input_state)(retro_input_state_t) = NULL;

	PA_INFO("Loading core %s\n", corefile);

	memset(&current_core, 0, sizeof(current_core));
	current_core.handle = dlopen(corefile, RTLD_LAZY);

	if (!current_core.handle) {
		PA_ERROR("Couldn't load core: %s\n", dlerror());
		return -1;
	}

	set_directories(core_name);
	set_overrides(core_name);

	current_core.retro_init = dlsym(current_core.handle, "retro_init");
	current_core.retro_deinit = dlsym(current_core.handle, "retro_deinit");
	current_core.retro_get_system_info = dlsym(current_core.handle, "retro_get_system_info");
	current_core.retro_get_system_av_info = dlsym(current_core.handle, "retro_get_system_av_info");
	current_core.retro_set_controller_port_device = dlsym(current_core.handle, "retro_set_controller_port_device");
	current_core.retro_reset = dlsym(current_core.handle, "retro_reset");
	current_core.retro_run = dlsym(current_core.handle, "retro_run");
	current_core.retro_serialize_size = dlsym(current_core.handle, "retro_serialize_size");
	current_core.retro_serialize = dlsym(current_core.handle, "retro_serialize");
	current_core.retro_unserialize = dlsym(current_core.handle, "retro_unserialize");
	current_core.retro_cheat_reset = dlsym(current_core.handle, "retro_cheat_reset");
	current_core.retro_cheat_set = dlsym(current_core.handle, "retro_cheat_set");
	current_core.retro_load_game = dlsym(current_core.handle, "retro_load_game");
	current_core.retro_load_game_special = dlsym(current_core.handle, "retro_load_game_special");
	current_core.retro_unload_game = dlsym(current_core.handle, "retro_unload_game");
	current_core.retro_get_region = dlsym(current_core.handle, "retro_get_region");
	current_core.retro_get_memory_data = dlsym(current_core.handle, "retro_get_memory_data");
	current_core.retro_get_memory_size = dlsym(current_core.handle, "retro_get_memory_size");

	set_environment = dlsym(current_core.handle, "retro_set_environment");
	set_video_refresh = dlsym(current_core.handle, "retro_set_video_refresh");
	set_audio_sample = dlsym(current_core.handle, "retro_set_audio_sample");
	set_audio_sample_batch = dlsym(current_core.handle, "retro_set_audio_sample_batch");
	set_input_poll = dlsym(current_core.handle, "retro_set_input_poll");
	set_input_state = dlsym(current_core.handle, "retro_set_input_state");

	set_environment(pa_environment);
	set_video_refresh(pa_video_refresh);
	set_audio_sample(pa_audio_sample);
	set_audio_sample_batch(pa_audio_sample_batch);
	set_input_poll(pa_input_poll);
	set_input_state(pa_input_state);

	current_core.retro_get_system_info(&info);
	if (info.valid_extensions)
		extensions = string_split(info.valid_extensions, '|');

	return 0;
}

void core_load(void) {
	current_core.retro_init();
	current_core.initialized = true;
	PA_INFO("Finished loading core\n");
}

int core_load_content(struct content *content) {
	struct retro_game_info game_info = {};
	struct retro_system_av_info av_info = {};
	int ret = -1;
	char cheats_path[MAX_PATH] = {0};

	if (core_load_game_info(content, &game_info)) {
		goto finish;
	}

	if (!current_core.retro_load_game(&game_info)) {
		PA_ERROR("Couldn't load content\n");
		goto finish;
	}

	content_based_name(content, cheats_path, sizeof(cheats_path), save_dir, "cheats/", ".cht");
	if (cheats_path[0] != '\0') {
		cheats = cheats_load(cheats_path);
		core_apply_cheats(cheats);
	}

	sram_read();
	rtc_read();

	if (!strcmp(core_name, "fmsx") && current_core.retro_set_controller_port_device) {
		/* fMSX works best with joypad + keyboard */
		current_core.retro_set_controller_port_device(0, RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0));
	}

	current_core.retro_get_system_av_info(&av_info);

	PA_INFO("Screen: %dx%d\n", av_info.geometry.base_width, av_info.geometry.base_height);
	PA_INFO("Audio sample rate: %f\n", av_info.timing.sample_rate);
	PA_INFO("Frame rate: %f\n", av_info.timing.fps);
	PA_INFO("Reported aspect ratio: %f\n", av_info.geometry.aspect_ratio);

	sample_rate = av_info.timing.sample_rate;
	frame_rate = av_info.timing.fps;
	aspect_ratio = av_info.geometry.aspect_ratio;

	video_set_geometry(&av_info.geometry);
	plat_reinit();

#ifdef MMENU
	content_based_name(content, save_template_path, MAX_PATH, save_dir, NULL, ".st%i");
#endif

	ret = 0;
finish:
	return ret;
}

void core_load_last_opened(char *buf, size_t len) {
	char filename[MAX_PATH];
	FILE *file;
	size_t count;

	if (!len)
		goto finish;

	snprintf(filename, MAX_PATH, "%s%s", config_dir, "last_opened.txt");
	file = fopen(filename, "r");

	if (!file)
		goto finish;

	count = fread(buf, 1, len - 1, file);
	buf[count] = '\0';

finish:
	if (file)
		fclose(file);
}

void core_save_last_opened(struct content *content) {
	char filename[MAX_PATH];
	FILE *file;
	size_t len = strlen(content->path);

	if (!len)
		goto finish;

	snprintf(filename, MAX_PATH, "%s%s", config_dir, "last_opened.txt");
	file = fopen(filename, "w");

	if (!file)
		goto finish;

	fwrite(content->path, 1, len, file);

finish:
	if (file)
		fclose(file);
}

void core_apply_cheats(struct cheats *cheats) {
	if (!cheats)
		return;

	if (!current_core.retro_cheat_reset || !current_core.retro_cheat_set)
		return;

	current_core.retro_cheat_reset();
	for (int i = 0; i < cheats->count; i++) {
		if (cheats->cheats[i].enabled) {
			current_core.retro_cheat_set(i, cheats->cheats[i].enabled, cheats->cheats[i].code);
		}
	}
}

void core_run_frame(void) {
	polled = 0;
	current_core.retro_run();
}

void core_unload_content(void) {
	sram_write();
	rtc_write();

	cheats_free(cheats);
	cheats = NULL;

	current_core.retro_unload_game();
	content_free(content);
	content = NULL;
}

const char **core_extensions(void) {
	if (extensions)
		return extensions->list;

	return NULL;
}

void core_unload(void) {
	if (current_core.initialized) {
		core_unload_content();
		current_core.retro_deinit();
		current_core.initialized = false;
	}
}

void core_close(void) {
	PA_INFO("Unloading core...\n");

	core_unload();
	string_list_free(extensions);
	extensions = NULL;

	options_free();
	video_deinit();

	if (current_core.handle) {
		dlclose(current_core.handle);
		current_core.handle = NULL;
	}
}
