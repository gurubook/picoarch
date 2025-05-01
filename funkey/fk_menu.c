/*
    FK - FunKey retro gaming console library
    Copyright (C) 2020-2021 Vincent Buso
    Copyright (C) 2020-2021 Michel Stempin

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Vincent Buso
    vincent.buso@funkey-project.com
    Michel Stempin
    michel.stempin@funkey-project.com
*/

/**
 *  @file FK_menu.c
 *  This is the menu API for the FunKey retro gaming console library
 */

#include "fk_menu.h"
#include "main.h"
#include "core.h"
#include "scale.h"
#include "plat.h"

/// -------------- DEFINES --------------

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

//#define MENU_DEBUG
#define MENU_ERROR

#ifdef MENU_DEBUG
#define MENU_DEBUG_PRINTF(...)   printf(__VA_ARGS__);
#else
#define MENU_DEBUG_PRINTF(...)
#endif //MENU_DEBUG

#ifdef MENU_ERROR
#define MENU_ERROR_PRINTF(...)   printf(__VA_ARGS__);
#else
#define MENU_ERROR_PRINTF(...)
#endif //MENU_ERROR

#define SCREEN_HORIZONTAL_SIZE      320 //RES_HW_SCREEN_HORIZONTAL
#define SCREEN_VERTICAL_SIZE        320 //RES_HW_SCREEN_VERTICAL

#define SCROLL_SPEED_PX             30
#define FPS_MENU                    60
#define ARROWS_PADDING              8

#define MENU_ZONE_WIDTH             SCREEN_HORIZONTAL_SIZE
#define MENU_ZONE_HEIGHT            SCREEN_VERTICAL_SIZE
#define MENU_BG_SQUARE_WIDTH        180
#define MENU_BG_SQUARE_HEIGHT       140

#define MENU_FONT_NAME_TITLE        "/usr/games/menu_resources/OpenSans-Bold.ttf"
#define MENU_FONT_SIZE_TITLE        22
#define MENU_FONT_NAME_INFO         "/usr/games/menu_resources/OpenSans-Bold.ttf"
#define MENU_FONT_SIZE_INFO         16
#define MENU_FONT_NAME_SMALL_INFO   "/usr/games/menu_resources/OpenSans-Regular.ttf"
#define MENU_FONT_SIZE_SMALL_INFO   13
#define MENU_PNG_BG_PATH            "/usr/games/menu_resources/zone_bg.png"
#define MENU_PNG_ARROW_TOP_PATH     "/usr/games/menu_resources/arrow_top.png"
#define MENU_PNG_ARROW_BOTTOM_PATH  "/usr/games/menu_resources/arrow_bottom.png"

#define GRAY_MAIN_R                 85
#define GRAY_MAIN_G                 85
#define GRAY_MAIN_B                 85
#define WHITE_MAIN_R                236
#define WHITE_MAIN_G                236
#define WHITE_MAIN_B                236

#define MAX_SAVE_SLOTS              9

#define MAXPATHLEN                  512


/// -------------- STATIC VARIABLES --------------
static SDL_Surface * background_screen = NULL;
static int backup_key_repeat_delay=0;
static int backup_key_repeat_interval=0;
static TTF_Font *menu_title_font = NULL;
static TTF_Font *menu_info_font = NULL;
static TTF_Font *menu_small_info_font = NULL;
static SDL_Surface *img_arrow_top = NULL;
static SDL_Surface *img_arrow_bottom = NULL;
static SDL_Surface ** menu_zone_surfaces = NULL;
static int *idx_menus = NULL;
static int nb_menu_zones = 0;
static int menuItem=0;
static int stop_menu_loop = 0;

static SDL_Color text_color = {GRAY_MAIN_R, GRAY_MAIN_G, GRAY_MAIN_B};
static int padding_y_from_center_menu_zone = 18;
static uint16_t width_progress_bar = 100;
static uint16_t height_progress_bar = 20;

#ifdef HAS_MENU_VOLUME
static uint16_t x_volume_bar = 0;
static uint16_t y_volume_bar = 0;
static int volume_percentage = 0;
#endif
#ifdef HAS_MENU_BRIGHTNESS
static uint16_t x_brightness_bar = 0;
static uint16_t y_brightness_bar = 0;
static int brightness_percentage = 0;
#endif

#ifdef HAS_MENU_ASPECT_RATIO
#undef X
#define X(a, b) b,
static const char *aspect_ratio_name[] = {ASPECT_RATIOS};
static int menu_aspect_ratio = ASPECT_RATIOS_TYPE_STRETCHED;
/* static int aspect_ratio_factor_percent = 50;
 * static int aspect_ratio_factor_step = 10; */
#endif

#ifdef HAS_MENU_THEME
static Configuration *config = NULL;
static int indexChooseLayout = 0;
#endif

#undef X
#define X(a, b) b,
const char *resume_options_str[] = {RESUME_OPTIONS};

/* #if defined(HAS_MENU_SAVE) || defined (HAS_MENU_LOAD)
 * static int savestate_slot = 0;
 * #endif */

#ifdef HAS_MENU_USB
/// USB stuff
static int usb_data_connected = 0;
static int usb_sharing = 0;
#endif

#ifdef HAS_MENU_RO_RW
static int read_write = 0;
#endif

/// -------------- FUNCTIONS IMPLEMENTATION --------------

#if defined(HAS_MENU_VOLUME) || defined(HAS_MENU_BRIGHTNESS)
static void draw_progress_bar(SDL_Surface * surface, uint16_t x, uint16_t y, uint16_t width,
		uint16_t height, uint8_t percentage, uint16_t nb_bars)
{
	/// ------ Init Variables ------
	uint16_t line_width = 1; //px
	uint16_t padding_bars_ratio = 3;
	uint16_t nb_full_bars = 0;

	/// ------ Check values ------
	percentage = (percentage > 100)?100:percentage;
	x = (x > (surface->w-1))?(surface->w-1):x;
	y = (y > surface->h-1)?(surface->h-1):y;
	width = (width < line_width*2+1)?(line_width*2+1):width;
	width = (width > surface->w-x-1)?(surface->w-x-1):width;
	height = (height < line_width*2+1)?(line_width*2+1):height;
	height = (height > surface->h-y-1)?(surface->h-y-1):height;
	uint16_t nb_bars_max = ( width * padding_bars_ratio  /  (line_width*2+1) + 1 ) / (padding_bars_ratio+1);
	nb_bars = (nb_bars > nb_bars_max)?nb_bars_max:nb_bars;
	uint16_t bar_width = (width / nb_bars)*padding_bars_ratio/(padding_bars_ratio+1)+1;
	uint16_t bar_padding_x = bar_width/padding_bars_ratio;
	nb_full_bars = nb_bars*percentage/100;

	/// ------ draw full bars ------
	for (int i = 0; i < nb_full_bars; ++i)
	{
		/// ---- draw one bar ----
		//MENU_DEBUG_PRINTF("Drawing filled bar %d\n", i);
		SDL_Rect rect = {x+ i*(bar_width +bar_padding_x),
				y, bar_width, height};
		SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, GRAY_MAIN_R, GRAY_MAIN_G, GRAY_MAIN_B));
	}

	/// ------ draw full bars ------
	for (int i = 0; i < (nb_bars-nb_full_bars); ++i)
	{
		/// ---- draw one bar ----
		//MENU_DEBUG_PRINTF("Drawing empty bar %d\n", i);
		SDL_Rect rect = {x+ i*(bar_width +bar_padding_x) + nb_full_bars*(bar_width +bar_padding_x),
				y, bar_width, height};
		SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, GRAY_MAIN_R, GRAY_MAIN_G, GRAY_MAIN_B));

		SDL_Rect rect2 = {x+ i*(bar_width +bar_padding_x) + line_width + nb_full_bars*(bar_width +bar_padding_x),
				y + line_width, bar_width - line_width*2, height - line_width*2};
		SDL_FillRect(surface, &rect2, SDL_MapRGB(surface->format, WHITE_MAIN_R, WHITE_MAIN_R, WHITE_MAIN_R));
	}


}
#endif

#if defined(HAS_MENU_ASPECT_RATIO)
static void read_aspect_ratio(void)
{
	switch (scale_size) {
	case SCALE_SIZE_FULL:
		menu_aspect_ratio = ASPECT_RATIOS_TYPE_STRETCHED;
		break;
	case SCALE_SIZE_ASPECT:
		menu_aspect_ratio = ASPECT_RATIOS_TYPE_SCALED;
		break;
	case SCALE_SIZE_CROP:
		menu_aspect_ratio = ASPECT_RATIOS_TYPE_CROPPED;
		break;
	case SCALE_SIZE_NONE:
		menu_aspect_ratio = ASPECT_RATIOS_TYPE_NONE;
		break;
	}
}

static void update_aspect_ratio(void)
{
	switch (menu_aspect_ratio) {
	case ASPECT_RATIOS_TYPE_STRETCHED:
		scale_size = SCALE_SIZE_FULL;
		scale_filter = SCALE_FILTER_SMOOTH;
		break;
	case ASPECT_RATIOS_TYPE_SCALED:
		scale_size = SCALE_SIZE_ASPECT;
		scale_filter = SCALE_FILTER_SMOOTH;
		break;
	case ASPECT_RATIOS_TYPE_CROPPED:
		scale_size = SCALE_SIZE_CROP;
		scale_filter = SCALE_FILTER_SMOOTH;
		break;
	case ASPECT_RATIOS_TYPE_NONE:
		scale_size = SCALE_SIZE_NONE;
		scale_filter = SCALE_FILTER_NEAREST;
		break;
	}
	scale_update_scaler();
}
#endif

static void add_menu_zone(ENUM_MENU_TYPE menu_type)
{
	/// ------ Increase nb of menu zones -------
	nb_menu_zones++;

	/// ------ Realoc idx Menus array -------
	if(!idx_menus){
		idx_menus = (int*) malloc(nb_menu_zones*sizeof(int));
		menu_zone_surfaces = (SDL_Surface**) malloc(nb_menu_zones*sizeof(SDL_Surface*));
	}
	else{
		int *temp = (int*) realloc(idx_menus, nb_menu_zones*sizeof(int));
		idx_menus = temp;
		menu_zone_surfaces = (SDL_Surface**) realloc(menu_zone_surfaces, nb_menu_zones*sizeof(SDL_Surface*));
	}
	idx_menus[nb_menu_zones-1] = menu_type;

	/// ------ Reinit menu surface with height increased -------
	menu_zone_surfaces[nb_menu_zones-1] = IMG_Load(MENU_PNG_BG_PATH);
	if(!menu_zone_surfaces[nb_menu_zones-1]) {
		MENU_ERROR_PRINTF("ERROR IMG_Load: %s\n", IMG_GetError());
	}

	/// --------- Init Common Variables --------
	SDL_Surface *text_surface = NULL;
	SDL_Surface *surface = menu_zone_surfaces[nb_menu_zones-1];
	SDL_Rect text_pos;

	/// --------- Add new zone ---------
	switch(menu_type){
#ifdef HAS_MENU_VOLUME
	case MENU_TYPE_VOLUME:
		MENU_DEBUG_PRINTF("Init MENU_TYPE_VOLUME\n");
		/// ------ Text ------
		text_surface = TTF_RenderText_Blended(menu_title_font, "VOLUME", text_color);
		text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
		text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 - padding_y_from_center_menu_zone;
		SDL_BlitSurface(text_surface, NULL, surface, &text_pos);

		x_volume_bar = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - width_progress_bar)/2;
		y_volume_bar = surface->h - MENU_ZONE_HEIGHT/2 - height_progress_bar/2 + padding_y_from_center_menu_zone;
		draw_progress_bar(surface, x_volume_bar, y_volume_bar,
				width_progress_bar, height_progress_bar, 0, 100/STEP_CHANGE_VOLUME);
		break;
#endif
#ifdef HAS_MENU_BRIGHTNESS
	case MENU_TYPE_BRIGHTNESS:
		MENU_DEBUG_PRINTF("Init MENU_TYPE_BRIGHTNESS\n");
		/// ------ Text ------
		text_surface = TTF_RenderText_Blended(menu_title_font, "BRIGHTNESS", text_color);
		text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
		text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 - padding_y_from_center_menu_zone;
		SDL_BlitSurface(text_surface, NULL, surface, &text_pos);

		x_brightness_bar = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - width_progress_bar)/2;
		y_brightness_bar = surface->h - MENU_ZONE_HEIGHT/2 - height_progress_bar/2 + padding_y_from_center_menu_zone;
		draw_progress_bar(surface, x_brightness_bar, y_brightness_bar,
				width_progress_bar, height_progress_bar, 0, 100/STEP_CHANGE_BRIGHTNESS);
		break;
#endif
#ifdef HAS_MENU_SAVE
	case MENU_TYPE_SAVE:
		MENU_DEBUG_PRINTF("Init MENU_TYPE_SAVE\n");
		/// ------ Text ------
		text_surface = TTF_RenderText_Blended(menu_title_font, "SAVE", text_color);
		text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
		text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 - padding_y_from_center_menu_zone*2;
		SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
		break;
#endif
#ifdef HAS_MENU_LOAD
	case MENU_TYPE_LOAD:
		MENU_DEBUG_PRINTF("Init MENU_TYPE_LOAD\n");
		/// ------ Text ------
		text_surface = TTF_RenderText_Blended(menu_title_font, "LOAD", text_color);
		text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
		text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 - padding_y_from_center_menu_zone*2;
		SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
		break;
#endif
#ifdef HAS_MENU_ASPECT_RATIO
	case MENU_TYPE_ASPECT_RATIO:
		MENU_DEBUG_PRINTF("Init MENU_TYPE_ASPECT_RATIO\n");
		/// ------ Text ------
		text_surface = TTF_RenderText_Blended(menu_title_font, "ASPECT RATIO", text_color);
		text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
		text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 - padding_y_from_center_menu_zone;
		SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
		break;
#endif
#ifdef HAS_MENU_USB
	case MENU_TYPE_USB:
		MENU_DEBUG_PRINTF("Init MENU_TYPE_USB\n");
		/// ------ Text ------
		text_surface = TTF_RenderText_Blended(menu_title_font, "USB", text_color);
		text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
		text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
		SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
		break;
#endif
#ifdef HAS_MENU_THEME
	case MENU_TYPE_THEME:
		MENU_DEBUG_PRINTF("Init MENU_TYPE_THEME\n");
		/// ------ Text ------
		text_surface = TTF_RenderText_Blended(menu_title_font, "SET THEME", text_color);
		text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
		text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 - padding_y_from_center_menu_zone*2;
		SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
		break;
#endif
#ifdef HAS_MENU_LAUNCHER
	case MENU_TYPE_LAUNCHER:
		MENU_DEBUG_PRINTF("Init MENU_TYPE_LAUNCHER\n");
		/// ------ Text ------
		text_surface = TTF_RenderText_Blended(menu_title_font, "SET LAUNCHER", text_color);
		text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
		text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 - padding_y_from_center_menu_zone*2;
		SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
		/// ------ Text ------
		text_surface = TTF_RenderText_Blended(menu_title_font, "GMENU2X", text_color);
		text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
		text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
		SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
		break;
#endif
#ifdef HAS_MENU_RO_RW
	case MENU_TYPE_RO_RW:
		MENU_DEBUG_PRINTF("Init MENU_TYPE_RO_RW\n");
		/// ------ Text ------
		text_surface = TTF_RenderText_Blended(menu_title_font, "SET SYSTEM:", text_color);
		text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
		text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 - padding_y_from_center_menu_zone*2;
		SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
	    break;
#endif
#ifdef HAS_MENU_ADVANCED
	case MENU_TYPE_ADVANCED:
		MENU_DEBUG_PRINTF("Init MENU_TYPE_ADVANCED\n");
		/// ------ Text ------
		text_surface = TTF_RenderText_Blended(menu_title_font, "ADVANCED", text_color);
		text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
		text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
		SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
		break;
#endif
#ifdef HAS_MENU_EXIT
	case MENU_TYPE_EXIT:
		MENU_DEBUG_PRINTF("Init MENU_TYPE_EXIT\n");
		/// ------ Text ------
		text_surface = TTF_RenderText_Blended(menu_title_font, "EXIT GAME", text_color);
		text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
		text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
		SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
		break;
#endif
#ifdef HAS_MENU_POWERDOWN
	case MENU_TYPE_POWERDOWN:
		MENU_DEBUG_PRINTF("Init MENU_TYPE_POWERDOWN\n");
		/// ------ Text ------
		text_surface = TTF_RenderText_Blended(menu_title_font, "POWERDOWN", text_color);
		text_pos.x = (surface->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
		text_pos.y = surface->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
		SDL_BlitSurface(text_surface, NULL, surface, &text_pos);
		break;
#endif
	default:
		MENU_DEBUG_PRINTF("Warning - In add_menu_zone, unknown MENU_TYPE: %d\n", menu_type);
		break;
	}

	/// ------ Free Surfaces -------
	SDL_FreeSurface(text_surface);
}

static void init_menu_zones(void)
{
#ifdef HAS_MENU_VOLUME
	add_menu_zone(MENU_TYPE_VOLUME);
#endif
#ifdef HAS_MENU_BRIGHTNESS
	add_menu_zone(MENU_TYPE_BRIGHTNESS);
#endif
#ifdef HAS_MENU_SAVE
	if (state_allowed()) {
		add_menu_zone(MENU_TYPE_SAVE);
	}
#endif
#ifdef HAS_MENU_LOAD
	if (state_allowed()) {
		add_menu_zone(MENU_TYPE_LOAD);
	}
#endif
#ifdef HAS_MENU_ASPECT_RATIO
	add_menu_zone(MENU_TYPE_ASPECT_RATIO);
#endif
#ifdef HAS_MENU_RO_RW
	add_menu_zone(MENU_TYPE_RO_RW);
#endif
#ifdef HAS_MENU_ADVANCED
	add_menu_zone(MENU_TYPE_ADVANCED);
#endif
#ifdef HAS_MENU_EXIT
	add_menu_zone(MENU_TYPE_EXIT);
#endif
#ifdef HAS_MENU_USB
	add_menu_zone(MENU_TYPE_USB);
#endif
#ifdef HAS_MENU_THEME
	add_menu_zone(MENU_TYPE_THEME);
#endif
#ifdef HAS_MENU_LAUNCHER
	add_menu_zone(MENU_TYPE_LAUNCHER);
#endif
#ifdef HAS_MENU_POWERDOWN
	add_menu_zone(MENU_TYPE_POWERDOWN);
#endif
}


#ifdef HAS_MENU_THEME
void FK_InitMenu(Configuration &c)
 #else
void FK_InitMenu(void)
#endif
{
	MENU_DEBUG_PRINTF("Init Menu\n");
	/// ----- Loading the fonts -----
	menu_title_font = TTF_OpenFont(MENU_FONT_NAME_TITLE, MENU_FONT_SIZE_TITLE);
	if(!menu_title_font){
		MENU_ERROR_PRINTF("ERROR in init_menu_SDL: Could not open menu font %s, %s\n", MENU_FONT_NAME_TITLE, SDL_GetError());
	}
	menu_info_font = TTF_OpenFont(MENU_FONT_NAME_INFO, MENU_FONT_SIZE_INFO);
	if(!menu_info_font){
		MENU_ERROR_PRINTF("ERROR in init_menu_SDL: Could not open menu font %s, %s\n", MENU_FONT_NAME_INFO, SDL_GetError());
	}
	menu_small_info_font = TTF_OpenFont(MENU_FONT_NAME_SMALL_INFO, MENU_FONT_SIZE_SMALL_INFO);
	if(!menu_small_info_font){
		MENU_ERROR_PRINTF("ERROR in init_menu_SDL: Could not open menu font %s, %s\n", MENU_FONT_NAME_SMALL_INFO, SDL_GetError());
	}

	/// ------ Load arrows imgs -------
	img_arrow_top = IMG_Load(MENU_PNG_ARROW_TOP_PATH);
	if(!img_arrow_top) {
		MENU_ERROR_PRINTF("ERROR IMG_Load: %s\n", IMG_GetError());
	}
	img_arrow_bottom = IMG_Load(MENU_PNG_ARROW_BOTTOM_PATH);
	if(!img_arrow_bottom) {
		MENU_ERROR_PRINTF("ERROR IMG_Load: %s\n", IMG_GetError());
	}

#ifdef HAS_MENU_THEME
	/// ------ Save config pointer ------
	config = &c;
#endif
#ifdef HAS_MENU_RO_RW
	/// ----- Shell cmd ----
	if (system(SHELL_CMD_RO) < 0) {
	  MENU_ERROR_PRINTF("Failed to run command %s\n", SHELL_CMD_RO);
	}
#endif

	/// ------ Init menu zones ------
	init_menu_zones();

	return;
}


void FK_EndMenu(void)
{
	MENU_DEBUG_PRINTF("End Menu \n");
	/// ------ Close font -------
	TTF_CloseFont(menu_title_font);
	TTF_CloseFont(menu_info_font);
	TTF_CloseFont(menu_small_info_font);

	/// ------ Free Surfaces -------
	for(int i=0; i < nb_menu_zones; i++){
		if(menu_zone_surfaces[i] != NULL){
			SDL_FreeSurface(menu_zone_surfaces[i]);
		}
	}
	SDL_FreeSurface(img_arrow_top);
	SDL_FreeSurface(img_arrow_bottom);

	/// ------ Free Menu memory and reset vars -----
	if(idx_menus){
		free(idx_menus);
	}
	idx_menus=NULL;
	nb_menu_zones = 0;

#ifdef HAS_MENU_RO_RW
	/// ----- Shell cmd ----
	if (system(SHELL_CMD_RO) < 0) {
		MENU_ERROR_PRINTF("Failed to run command %s\n", SHELL_CMD_RO);
	}
#endif

	return;
}

void FK_StopMenu(void)
{
	stop_menu_loop = 1;
}


static void init_menu_system_values(void)
{
#if defined(HAS_MENU_VOLUME) || defined(HAS_MENU_BRIGHTNESS) || defined(HAS_MENU_USB) || defined(HAS_MENU_RO_RW)
	FILE *fp;
	char res[100];
#endif

#ifdef HAS_MENU_VOLUME
	/// ------- Get system volume percentage --------
	fp = popen(SHELL_CMD_VOLUME_GET, "r");
	if (fp == NULL) {
		MENU_ERROR_PRINTF("Failed to run command %s\n", SHELL_CMD_VOLUME_GET );
		volume_percentage = 50; ///wrong value: setting default to 50
	}
	else{
		fgets(res, sizeof(res)-1, fp);
		pclose(fp);

		/// Check if Volume is a number (at least the first char)
		if(res[0] < '0' || res[0] > '9'){
			MENU_ERROR_PRINTF("Wrong return value: %s for volume cmd: %s\n",res, SHELL_CMD_VOLUME_GET);
			volume_percentage = 50; ///wrong value: setting default to 50
		}
		else{
			volume_percentage = atoi(res);
			MENU_DEBUG_PRINTF("System volume = %d%%\n", volume_percentage);
		}
	}
#endif
#ifdef HAS_MENU_BRIGHTNESS
	/// ------- Get system brightness percentage -------
	fp = popen(SHELL_CMD_BRIGHTNESS_GET, "r");
	if (fp == NULL) {
		MENU_ERROR_PRINTF("Failed to run command %s\n", SHELL_CMD_BRIGHTNESS_GET );
		brightness_percentage = 50; ///wrong value: setting default to 50
	}
	else{
		fgets(res, sizeof(res)-1, fp);
		pclose(fp);

		/// Check if brightness is a number (at least the first char)
		if(res[0] < '0' || res[0] > '9'){
			MENU_ERROR_PRINTF("Wrong return value: %s for volume cmd: %s\n",res, SHELL_CMD_BRIGHTNESS_GET);
			brightness_percentage = 50; ///wrong value: setting default to 50
		}
		else{
			brightness_percentage = atoi(res);
			MENU_DEBUG_PRINTF("System brightness = %d%%\n", brightness_percentage);
		}
	}
#endif
#ifdef HAS_MENU_USB
	/// ------- Get USB Value -------
	usb_data_connected = Utils::executeRawPath(SHELL_CMD_USB_DATA_CONNECTED);
	usb_sharing = Utils::executeRawPath(SHELL_CMD_USB_CHECK_IS_SHARING);

	/** Sanity check if usb not connected */
	if(!usb_data_connected){
		usb_sharing = 0;

		if(idx_menus[menuItem] == MENU_TYPE_USB){
			menuItem = 0;
		}
	}

	/** Sanity check if currently in USB sharing (should not happen) */
	if(usb_sharing){

		/// Force USB menu to launch
		for(int cur_idx=0; cur_idx < nb_menu_zones; cur_idx++){
			if(idx_menus[cur_idx] == MENU_TYPE_USB){
				menuItem = cur_idx;
				printf("USB mounted, setting menu item to %d\n", menuItem);
				break;
			}
		}
	}
#endif
}

static void menu_screen_refresh(SDL_Surface *screen, int menuItem, int prevItem, int scroll, uint8_t menu_confirmation, uint8_t menu_action)
{
	/// --------- Vars ---------
#ifdef HAS_MENU_USB
	int print_arrows = (scroll || usb_sharing)?0:1;
#else
	int print_arrows = 1;
#endif

	/// --------- Clear HW screen ----------
	if(SDL_BlitSurface(background_screen, NULL, screen, NULL)){
		MENU_ERROR_PRINTF("ERROR Could not Clear screen: %s\n", SDL_GetError());
	}

	/// --------- Setup Blit Window ----------
	SDL_Rect menu_blit_window;
	menu_blit_window.x = 0;
	menu_blit_window.w = SCREEN_HORIZONTAL_SIZE;

	/// --------- Blit prev menu Zone going away ----------
	menu_blit_window.y = scroll;
	menu_blit_window.h = SCREEN_VERTICAL_SIZE;
	if(SDL_BlitSurface(menu_zone_surfaces[prevItem], &menu_blit_window, screen, NULL)){
		MENU_ERROR_PRINTF("ERROR Could not Blit surface on screen: %s\n", SDL_GetError());
	}

	/// --------- Blit new menu Zone going in (only during animations) ----------
	if(scroll>0){
		menu_blit_window.y = SCREEN_VERTICAL_SIZE-scroll;
		menu_blit_window.h = SCREEN_VERTICAL_SIZE;
		if(SDL_BlitSurface(menu_zone_surfaces[menuItem], NULL, screen, &menu_blit_window)){
			MENU_ERROR_PRINTF("ERROR Could not Blit surface on screen: %s\n", SDL_GetError());
		}
	}
	else if(scroll<0){
		menu_blit_window.y = SCREEN_VERTICAL_SIZE+scroll;
		menu_blit_window.h = SCREEN_VERTICAL_SIZE;
		if(SDL_BlitSurface(menu_zone_surfaces[menuItem], &menu_blit_window, screen, NULL)){
			MENU_ERROR_PRINTF("ERROR Could not Blit surface on screen: %s\n", SDL_GetError());
		}
	}
	/// --------- No Scroll ? Blitting menu-specific info
	else{
		SDL_Surface * text_surface = NULL;
		char text_tmp[100];
		SDL_Rect text_pos;
#ifdef HAS_MENU_THEME
		char *curLayoutName;
		bool dots=false;
		int max_chars = 15;
#endif

		switch(idx_menus[menuItem]){
#ifdef HAS_MENU_VOLUME
		case MENU_TYPE_VOLUME:
			draw_progress_bar(screen, x_volume_bar, y_volume_bar,
					width_progress_bar, height_progress_bar, volume_percentage, 100/STEP_CHANGE_VOLUME);
			break;
#endif
#ifdef HAS_MENU_BRIGHTNESS
		case MENU_TYPE_BRIGHTNESS:
			draw_progress_bar(screen, x_volume_bar, y_volume_bar,
					width_progress_bar, height_progress_bar, brightness_percentage, 100/STEP_CHANGE_BRIGHTNESS);
			break;
#endif
#ifdef HAS_MENU_SAVE
		case MENU_TYPE_SAVE:
			/// ---- Write slot -----
			sprintf(text_tmp, "IN SLOT   < %d >", state_slot+1);
			text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
			text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
			text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
			SDL_BlitSurface(text_surface, NULL, screen, &text_pos);

			if(menu_action){
				sprintf(text_tmp, "Saving...");
				text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
			}
			else{
				if(menu_confirmation){
					sprintf(text_tmp, "Are you sure?");
					text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
				}
				else{
					if (state_exists(state_slot)) {
						text_surface = TTF_RenderText_Blended(menu_info_font, "Used", text_color);
					} else {
						text_surface = TTF_RenderText_Blended(menu_info_font, "Free", text_color);
					}
				}
			}
			text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
			text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
			SDL_BlitSurface(text_surface, NULL, screen, &text_pos);
			break;
#endif
#ifdef HAS_MENU_LOAD
		case MENU_TYPE_LOAD:
			/// ---- Write slot -----
			sprintf(text_tmp, "FROM SLOT   < %d >", state_slot+1);
			text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
			text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
			text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
			SDL_BlitSurface(text_surface, NULL, screen, &text_pos);

			if(menu_action){
				sprintf(text_tmp, "Loading...");
				text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
			}
			else{
				if(menu_confirmation){
					sprintf(text_tmp, "Are you sure?");
					text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
				}
				else{
					if (state_exists(state_slot)) {
						text_surface = TTF_RenderText_Blended(menu_info_font, "Used", text_color);
					} else {
						text_surface = TTF_RenderText_Blended(menu_info_font, "Free", text_color);
					}
				}
			}
			text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
			text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
			SDL_BlitSurface(text_surface, NULL, screen, &text_pos);
			break;
#endif
#ifdef HAS_MENU_ASPECT_RATIO
		case MENU_TYPE_ASPECT_RATIO:
			sprintf(text_tmp, "<   %s   >", aspect_ratio_name[menu_aspect_ratio]);
			text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
			text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
			text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + padding_y_from_center_menu_zone;
			SDL_BlitSurface(text_surface, NULL, screen, &text_pos);
			break;
#endif
#ifdef HAS_MENU_USB
		case MENU_TYPE_USB:
			/// ---- Write slot -----
			sprintf(text_tmp, "%s USB", usb_sharing?"EJECT":"MOUNT");
			text_surface = TTF_RenderText_Blended(menu_title_font, text_tmp, text_color);
			text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
			text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
			SDL_BlitSurface(text_surface, NULL, screen, &text_pos);

			if(menu_action){
				sprintf(text_tmp, "in progress ...");
				text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
				text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
				text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
				SDL_BlitSurface(text_surface, NULL, screen, &text_pos);
			}
			else if(menu_confirmation){
				sprintf(text_tmp, "Are you sure?");
				text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
				text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
				text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
				SDL_BlitSurface(text_surface, NULL, screen, &text_pos);
			}
			else{
					///Nothing
			}
			break;
#endif
#ifdef HAS_MENU_THEME
		case MENU_TYPE_THEME:
			/// ---- Write current chosen theme -----
			curLayoutName = (char*)Utils::getFileName(config->layouts_.at(indexChooseLayout)).c_str();

			// no more than max_chars chars in name to fit screen
			if(strlen(curLayoutName) > max_chars){
				curLayoutName[max_chars-2] = 0;
				dots = true;
			}
			sprintf(text_tmp, "< %s%s >", curLayoutName, dots?"...":"" );

			text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
			text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
			text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
			SDL_BlitSurface(text_surface, NULL, screen, &text_pos);

			if(menu_action){
				sprintf(text_tmp, "In progress...");
				text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
				text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
				text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
				SDL_BlitSurface(text_surface, NULL, screen, &text_pos);
			}
			else if(menu_confirmation){
				sprintf(text_tmp, "Are you sure?");
				text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
				text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
				text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
				SDL_BlitSurface(text_surface, NULL, screen, &text_pos);
			}
			break;
#endif
#ifdef HAS_MENU_LAUNCHER
		case MENU_TYPE_LAUNCHER:
			if(menu_action){
				sprintf(text_tmp, "In progress...");
				text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
				text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
				text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
				SDL_BlitSurface(text_surface, NULL, screen, &text_pos);
			}
			else if(menu_confirmation){
				sprintf(text_tmp, "Are you sure?");
				text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
				text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
				text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
				SDL_BlitSurface(text_surface, NULL, screen, &text_pos);
			}
			break;
#endif
#ifdef HAS_MENU_EXIT
		case MENU_TYPE_EXIT:
#endif
#ifdef HAS_MENU_POWERDOWN
		case MENU_TYPE_POWERDOWN:
#endif
#if defined(HAS_MENU_EXIT) || defined(HAS_MENU_POWERDOWN)
			if(menu_action){
				sprintf(text_tmp, "Shutting down...");
				text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
				text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
				text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
				SDL_BlitSurface(text_surface, NULL, screen, &text_pos);
			}
			else{
				if(menu_confirmation){
					sprintf(text_tmp, "Are you sure?");
					text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
					text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
					text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
					SDL_BlitSurface(text_surface, NULL, screen, &text_pos);
				}
			}
			break;
#endif
#ifdef HAS_MENU_RO_RW
		case MENU_TYPE_RO_RW:
			sprintf(text_tmp, "%s", read_write?"READ-ONLY":"READ-WRITE");
			text_surface = TTF_RenderText_Blended(menu_title_font, text_tmp, text_color);
			text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
			text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
			SDL_BlitSurface(text_surface, NULL, screen, &text_pos);

			if(menu_action){
				sprintf(text_tmp, "in progress ...");
				text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
				text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
				text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
				SDL_BlitSurface(text_surface, NULL, screen, &text_pos);
			}
			else if(menu_confirmation){
				sprintf(text_tmp, "Are you sure?");
				text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
				text_pos.x = (screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
				text_pos.y = screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
				SDL_BlitSurface(text_surface, NULL, screen, &text_pos);
			}
			else{
					///Nothing
			}
			break;
#endif
		default:
			break;
		}

		/// ------ Free Surfaces -------
		if(text_surface)
			SDL_FreeSurface(text_surface);
	}

	/// --------- Print arrows --------
	if(print_arrows){
		/// Top arrow
		SDL_Rect pos_arrow_top;
		pos_arrow_top.x = (screen->w - img_arrow_top->w)/2;
		pos_arrow_top.y = (screen->h - MENU_BG_SQUARE_HEIGHT)/4 - img_arrow_top->h/2;
		SDL_BlitSurface(img_arrow_top, NULL, screen, &pos_arrow_top);

		/// Bottom arrow
		SDL_Rect pos_arrow_bottom;
		pos_arrow_bottom.x = (screen->w - img_arrow_bottom->w)/2;
		pos_arrow_bottom.y = screen->h -
			(screen->h - MENU_BG_SQUARE_HEIGHT)/4 - img_arrow_bottom->h/2;
		SDL_BlitSurface(img_arrow_bottom, NULL, screen, &pos_arrow_bottom);
	}

	/// --------- Flip Screen ----------
	SDL_Flip(screen);
}


int FK_RunMenu(SDL_Surface *screen)
{
	MENU_DEBUG_PRINTF("Run Menu\n");

	SDL_Event event;
	uint32_t prev_ms = SDL_GetTicks();
	uint32_t cur_ms = SDL_GetTicks();
	int scroll=0;
	int start_scroll=0;
	uint8_t screen_refresh = 1;
	char shell_cmd[100];
	uint8_t menu_confirmation = 0;
	stop_menu_loop = 0;
#ifdef HAS_MENU_THEME
	indexChooseLayout = config->currentLayoutIdx_;
#endif
	int returnCode = MENU_RETURN_OK;

	/// ------ Load default keymap ------
	system(SHELL_CMD_KEYMAP_DEFAULT);

	/// ------ Get System values -------
	init_menu_system_values();
	int prevItem=menuItem;

	/// Save prev key repeat params and set new Key repeat
	SDL_GetKeyRepeat(&backup_key_repeat_delay, &backup_key_repeat_interval);
	if(SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL)){
		MENU_ERROR_PRINTF("ERROR with SDL_EnableKeyRepeat: %s\n", SDL_GetError());
	}

#if defined(HAS_MENU_SAVE) || defined(HAS_MENU_LOAD)
	/// Get save slot from game
	state_slot = (state_slot%MAX_SAVE_SLOTS); // security
#endif

#if defined(HAS_MENU_ASPECT_RATIO)
	read_aspect_ratio();
#endif

	plat_video_menu_enter(1);

	/// ------ Backup currently displayed app screen -------
	background_screen = SDL_CreateRGBSurface(SDL_SWSURFACE,
			screen->w, screen->h, 32, 0, 0, 0, 0);
	if(background_screen == NULL){
		MENU_ERROR_PRINTF("ERROR Could not create background_screen: %s\n", SDL_GetError());
		return MENU_RETURN_ERROR;
	}
	if(SDL_BlitSurface(screen, NULL, background_screen, NULL)){
		MENU_ERROR_PRINTF("ERROR Could not copy screen: %s\n", SDL_GetError());
	}

	/// -------- Main loop ---------
	while (!stop_menu_loop)
	{
		/// -------- Handle Keyboard Events ---------
		if(!scroll){
			while (SDL_PollEvent(&event))
				switch(event.type)
				{
				case SDL_QUIT:
					stop_menu_loop = 1;
					returnCode = MENU_RETURN_EXIT;
					break;
				case SDL_KEYDOWN:
					switch (event.key.keysym.sym)
					{
					case SDLK_b:
						if(menu_confirmation){
							/// ------ Reset menu confirmation ------
							menu_confirmation = 0;
							/// ------ Refresh screen ------
							screen_refresh = 1;
						}
						/*else{
							stop_menu_loop = 1;
						}*/
						break;

					case SDLK_q:
					case SDLK_ESCAPE:
						/// ------ Check if no action ------
#ifdef HAS_MENU_USB
						if(usb_sharing){
							break;
						}
#endif
						stop_menu_loop = 1;
						break;

					case SDLK_d:
					case SDLK_DOWN:
						MENU_DEBUG_PRINTF("DOWN\n");
						/// ------ Check if no action ------
#ifdef HAS_MENU_USB
						if(usb_sharing){
							break;
						}
#endif
						/// ------ Start scrolling to new menu -------
						menuItem++;
						if (menuItem>=nb_menu_zones) menuItem=0;

#ifdef HAS_MENU_USB
						/// Skip if usb menu if usb not connected
						if(idx_menus[menuItem] == MENU_TYPE_USB && !usb_data_connected){
							menuItem++;
							if (menuItem>=nb_menu_zones) menuItem=0;
						}
#endif
						start_scroll=1;

						/// ------ Reset menu confirmation ------
						menu_confirmation = 0;

						/// ------ Refresh screen ------
						screen_refresh = 1;
						break;

					case SDLK_u:
					case SDLK_UP:
						MENU_DEBUG_PRINTF("UP\n");
						/// ------ Check if no action ------
#ifdef HAS_MENU_USB
						if(usb_sharing){
							break;
						}
#endif
						/// ------ Start scrolling to new menu -------
						menuItem--;
						if (menuItem<0) menuItem=nb_menu_zones-1;

#ifdef HAS_MENU_USB
						/// Skip if usb menu if usb not connected
						if(idx_menus[menuItem] == MENU_TYPE_USB && !usb_data_connected){
							menuItem--;
							if (menuItem<0) menuItem=nb_menu_zones-1;
						}
#endif
						start_scroll=-1;

						/// ------ Reset menu confirmation ------
						menu_confirmation = 0;

						/// ------ Refresh screen ------
						screen_refresh = 1;
						break;

					case SDLK_l:
					case SDLK_LEFT:
						//MENU_DEBUG_PRINTF("LEFT\n");
#ifdef HAS_MENU_VOLUME
						if(idx_menus[menuItem] == MENU_TYPE_VOLUME){
							MENU_DEBUG_PRINTF("Volume DOWN\n");
							/// ----- Compute new value -----
							volume_percentage = (volume_percentage < STEP_CHANGE_VOLUME)?
									0:(volume_percentage-STEP_CHANGE_VOLUME);

							/// ----- Shell cmd ----
							sprintf(shell_cmd, "%s %d", SHELL_CMD_VOLUME_SET, volume_percentage);
							system(shell_cmd);

							/// ------ Refresh screen ------
							screen_refresh = 1;
						} else
#endif
#ifdef HAS_MENU_BRIGHTNESS
						  if(idx_menus[menuItem] == MENU_TYPE_BRIGHTNESS){
							MENU_DEBUG_PRINTF("Brightness DOWN\n");
							/// ----- Compute new value -----
							brightness_percentage = (brightness_percentage < STEP_CHANGE_BRIGHTNESS)?
									0:(brightness_percentage-STEP_CHANGE_BRIGHTNESS);

							/// ----- Shell cmd ----
							sprintf(shell_cmd, "%s %d", SHELL_CMD_BRIGHTNESS_SET, brightness_percentage);
							system(shell_cmd);

							/// ------ Refresh screen ------
							screen_refresh = 1;
						} else
#endif
#ifdef HAS_MENU_LOAD
						    if(idx_menus[menuItem] == MENU_TYPE_SAVE){
							MENU_DEBUG_PRINTF("Save Slot DOWN\n");
							state_slot = (!state_slot)?(MAX_SAVE_SLOTS-1):(state_slot-1);
							/// ------ Refresh screen ------
							screen_refresh = 1;
						} else
#endif
#ifdef HAS_MENU_LOAD
						      if(idx_menus[menuItem] == MENU_TYPE_LOAD){
							MENU_DEBUG_PRINTF("Load Slot DOWN\n");
							//idx_load_slot = (!idx_load_slot)?(MAX_SAVE_SLOTS-1):(idx_load_slot-1);
							state_slot = (!state_slot)?(MAX_SAVE_SLOTS-1):(state_slot-1);
							/// ------ Refresh screen ------
							screen_refresh = 1;
						} else
#endif
#ifdef HAS_MENU_ASPECT_RATIO
						  if(idx_menus[menuItem] == MENU_TYPE_ASPECT_RATIO){
							MENU_DEBUG_PRINTF("Aspect Ratio DOWN\n");
							menu_aspect_ratio = (!menu_aspect_ratio)?(NB_ASPECT_RATIOS_TYPES-1):(menu_aspect_ratio-1);
							update_aspect_ratio();

							/// ------ Refresh screen ------
							screen_refresh = 1;
						} else
#endif
#ifdef HAS_MENU_THEME
						  if(idx_menus[menuItem] == MENU_TYPE_THEME){
							MENU_DEBUG_PRINTF("Theme previous\n");
							indexChooseLayout = (!indexChooseLayout)?(config->layouts_.size()-1):(indexChooseLayout-1);
							/// ------ Refresh screen ------
							screen_refresh = 1;
						} else
#endif
						    {}
						break;

					case SDLK_r:
					case SDLK_RIGHT:
						//MENU_DEBUG_PRINTF("RIGHT\n");
#ifdef HAS_MENU_VOLUME
						if(idx_menus[menuItem] == MENU_TYPE_VOLUME){
							MENU_DEBUG_PRINTF("Volume UP\n");
							/// ----- Compute new value -----
							volume_percentage = (volume_percentage > 100 - STEP_CHANGE_VOLUME)?
									100:(volume_percentage+STEP_CHANGE_VOLUME);

							/// ----- Shell cmd ----
							sprintf(shell_cmd, "%s %d", SHELL_CMD_VOLUME_SET, volume_percentage);
							system(shell_cmd);

							/// ------ Refresh screen ------
							screen_refresh = 1;
						} else
#endif
#ifdef HAS_MENU_BRIGHTNESS
						  if(idx_menus[menuItem] == MENU_TYPE_BRIGHTNESS){
							MENU_DEBUG_PRINTF("Brightness UP\n");
							/// ----- Compute new value -----
							brightness_percentage = (brightness_percentage > 100 - STEP_CHANGE_BRIGHTNESS)?
									100:(brightness_percentage+STEP_CHANGE_BRIGHTNESS);

							/// ----- Shell cmd ----
							sprintf(shell_cmd, "%s %d", SHELL_CMD_BRIGHTNESS_SET, brightness_percentage);
							system(shell_cmd);

							/// ------ Refresh screen ------
							screen_refresh = 1;
						} else
#endif
#ifdef HAS_MENU_SAVE
						  if(idx_menus[menuItem] == MENU_TYPE_SAVE){
							MENU_DEBUG_PRINTF("Save Slot UP\n");
							state_slot = (state_slot+1)%MAX_SAVE_SLOTS;
							/// ------ Refresh screen ------
							screen_refresh = 1;
						} else
#endif
#ifdef HAS_MENU_LOAD
						  if(idx_menus[menuItem] == MENU_TYPE_LOAD){
							MENU_DEBUG_PRINTF("Load Slot UP\n");
							//idx_load_slot = (idx_load_slot+1)%MAX_SAVE_SLOTS;
							state_slot = (state_slot+1)%MAX_SAVE_SLOTS;
							/// ------ Refresh screen ------
							screen_refresh = 1;
						} else
#endif
#ifdef HAS_MENU_ASPECT_RATIO
						  if(idx_menus[menuItem] == MENU_TYPE_ASPECT_RATIO){
							MENU_DEBUG_PRINTF("Aspect Ratio UP\n");
							menu_aspect_ratio = (menu_aspect_ratio+1)%NB_ASPECT_RATIOS_TYPES;
							update_aspect_ratio();
							/// ------ Refresh screen ------
							screen_refresh = 1;
						} else
#endif
#ifdef HAS_MENU_THEME
						    if(idx_menus[menuItem] == MENU_TYPE_THEME){
							MENU_DEBUG_PRINTF("Theme previous\n");
							indexChooseLayout = (indexChooseLayout+1)%config->layouts_.size();
							/// ------ Refresh screen ------
							screen_refresh = 1;
						} else
#endif
						      {}						      
						break;

					case SDLK_a:
					case SDLK_RETURN:
#ifdef HAS_MENU_SAVE
						if(idx_menus[menuItem] == MENU_TYPE_SAVE){
							if(menu_confirmation){
								MENU_DEBUG_PRINTF("Saving in slot %d\n", state_slot);
								/// ------ Refresh Screen -------
								menu_screen_refresh(screen, menuItem, prevItem, scroll, menu_confirmation, 1);

								/// ------ Save game ------
								state_write();
								stop_menu_loop = 1;
							}
							else{
								MENU_DEBUG_PRINTF("Save game - asking confirmation\n");
								menu_confirmation = 1;
								/// ------ Refresh screen ------
								screen_refresh = 1;
							}
						} else
#endif
#ifdef HAS_MENU_LOAD
						  if(idx_menus[menuItem] == MENU_TYPE_LOAD){
							if(menu_confirmation){
								MENU_DEBUG_PRINTF("Loading in slot %d\n", state_slot);
								/// ------ Refresh Screen -------
								menu_screen_refresh(screen, menuItem, prevItem, scroll, menu_confirmation, 1);

								/// ------ Load game ------
								state_read();
								stop_menu_loop = 1;
							}
							else{
								MENU_DEBUG_PRINTF("Save game - asking confirmation\n");
								menu_confirmation = 1;
								/// ------ Refresh screen ------
								screen_refresh = 1;
							}
						} else
#endif
#ifdef HAS_MENU_USB
						    if(idx_menus[menuItem] == MENU_TYPE_USB){
							MENU_DEBUG_PRINTF("USB %s\n", usb_sharing?"unmount":"mount");
							if(menu_confirmation){
								MENU_DEBUG_PRINTF("%s USB - confirmed\n", usb_sharing?"Unmount":"Mount");
								/// ----- Refresh screen ----
								menu_screen_refresh(screen, menuItem, prevItem, scroll, menu_confirmation, 1);

								/// ----- Shell cmd ----
								/*system(usb_sharing?SHELL_CMD_USB_UNMOUNT:SHELL_CMD_USB_MOUNT);*/
								bool res = Utils::executeRawPath(usb_sharing?SHELL_CMD_USB_UNMOUNT:SHELL_CMD_USB_MOUNT);
								if (!res) {
									MENU_ERROR_PRINTF("Failed to run command %s\n", shell_cmd);
								}
								else{
									usb_sharing = !usb_sharing;
								}

								/// ------ Refresh screen ------
								menu_confirmation = 0;
								screen_refresh = 1;
							}
							else{
								MENU_DEBUG_PRINTF("%s USB - asking confirmation\n", usb_sharing?"Unmount":"Mount");
								menu_confirmation = 1;
								screen_refresh = 1;
							}
						} else
#endif
#ifdef HAS_MENU_THEME
						  if(idx_menus[menuItem] == MENU_TYPE_THEME){
							if(menu_confirmation){
								MENU_DEBUG_PRINTF("Theme change - confirmed\n");

								/// ------ Refresh Screen -------
								menu_screen_refresh(screen, menuItem, prevItem, scroll, menu_confirmation, 1);

								/// ----- Write new theme and restart RetroFe ----
								config->exportCurrentLayout(Utils::combinePath(Configuration::absolutePath, "layout.conf"),
										Utils::getFileName(config->layouts_.at(indexChooseLayout)));
								stop_menu_loop = 1;
								returnCode = MENU_RETURN_EXIT;
							}
							else{
								MENU_DEBUG_PRINTF("Theme change - asking confirmation\n");
								menu_confirmation = 1;
								/// ------ Refresh screen ------
								screen_refresh = 1;
							}
						} else
#endif
#ifdef HAS_MENU_LAUNCHER
						  if(idx_menus[menuItem] == MENU_TYPE_LAUNCHER){
							if(menu_confirmation){
								MENU_DEBUG_PRINTF("Lancher change - confirmed\n");

								/// ------ Refresh Screen -------
								menu_screen_refresh(screen, menuItem, prevItem, scroll, menu_confirmation, 1);

								/// ----- Shell cmd ----
								MENU_DEBUG_PRINTF("Running command: %s\n", SHELL_CMD_SET_LAUNCHER_GMENU2X);
								Utils::executeRawPath(SHELL_CMD_SET_LAUNCHER_GMENU2X);

								stop_menu_loop = 1;
								returnCode = MENU_RETURN_EXIT;
							}
							else{
								MENU_DEBUG_PRINTF("Launcher change - asking confirmation\n");
								menu_confirmation = 1;
								/// ------ Refresh screen ------
								screen_refresh = 1;
							}
						} else
#endif
#ifdef HAS_MENU_ADVANCED
						if(idx_menus[menuItem] == MENU_TYPE_ADVANCED){
							MENU_DEBUG_PRINTF("Advanced menu\n");

							SDL_Event sdlevent;
							do {
								SDL_WaitEvent(&sdlevent);
							} while (sdlevent.type != SDL_KEYUP || sdlevent.key.keysym.sym != event.key.keysym.sym);

							stop_menu_loop = 1;
							returnCode = MENU_RETURN_MENU;
						} else
#endif
#ifdef HAS_MENU_EXIT
						  if(idx_menus[menuItem] == MENU_TYPE_EXIT){
							MENU_DEBUG_PRINTF("Exit game\n");
							if(menu_confirmation){
								MENU_DEBUG_PRINTF("Exit game - confirmed\n");
								/// ----- The game should be saved here ----

								/// ----- Exit game and back to launcher ----
								stop_menu_loop = 1;
								returnCode = MENU_RETURN_EXIT;
							}
							else{
								MENU_DEBUG_PRINTF("Exit game - asking confirmation\n");
								menu_confirmation = 1;
								/// ------ Refresh screen ------
								screen_refresh = 1;
							}
						} else
#endif
#ifdef HAS_MENU_POWERDOWN
						  if(idx_menus[menuItem] == MENU_TYPE_POWERDOWN){
							if(menu_confirmation){
								MENU_DEBUG_PRINTF("Powerdown - confirmed\n");

								/// ------ Refresh Screen -------
								menu_screen_refresh(screen, menuItem, prevItem, scroll, menu_confirmation, 1);

								/// ----- Shell cmd ----
								system(SHELL_CMD_POWERDOWN);
								return MENU_RETURN_EXIT;
							}
							else{
								MENU_DEBUG_PRINTF("Powerdown - asking confirmation\n");
								menu_confirmation = 1;
								/// ------ Refresh screen ------
								screen_refresh = 1;
							}
						} else
#endif
#ifdef HAS_MENU_RO_RW
						    if(idx_menus[menuItem] == MENU_TYPE_RO_RW){
							MENU_DEBUG_PRINTF("%s\n", read_write?"RO":"RW");
							if(menu_confirmation){
								MENU_DEBUG_PRINTF("SYSTEM %s - confirmed\n", read_write?"RO":"RW");
								/// ----- Refresh screen ----
								menu_screen_refresh(screen, menuItem, prevItem, scroll, menu_confirmation, 1);

								/// ----- Shell cmd ----
								if (system(read_write?SHELL_CMD_RO:SHELL_CMD_RW) < 0) {
									MENU_ERROR_PRINTF("Failed to run command %s\n", shell_cmd);
								}
								else{
									read_write = !read_write;
								}

								/// ------ Refresh screen ------
								menu_confirmation = 0;
								screen_refresh = 1;
							}
							else{
								MENU_DEBUG_PRINTF("SYSTEM %s - asking confirmation\n", read_write?"RW":"RO");
								menu_confirmation = 1;
								screen_refresh = 1;
							}
						} else
#endif
						    {}
						break;

					default:
						//MENU_DEBUG_PRINTF("Keydown: %d\n", event.key.keysym.sym);
						break;
					}
					break;
				}
		}

		/// --------- Handle Scroll effect ---------
		if ((scroll>0) || (start_scroll>0)){
			scroll+=MIN(SCROLL_SPEED_PX, MENU_ZONE_HEIGHT-scroll);
			start_scroll = 0;
			screen_refresh = 1;
		}
		else if ((scroll<0) || (start_scroll<0)){
			scroll-=MIN(SCROLL_SPEED_PX, MENU_ZONE_HEIGHT+scroll);
			start_scroll = 0;
			screen_refresh = 1;
		}
		if (scroll>=MENU_ZONE_HEIGHT || scroll<=-MENU_ZONE_HEIGHT) {
			prevItem=menuItem;
			scroll=0;
			screen_refresh = 1;
		}

		/// --------- Handle FPS ---------
		cur_ms = SDL_GetTicks();
		if(cur_ms-prev_ms < 1000/FPS_MENU){
			SDL_Delay(1000/FPS_MENU - (cur_ms-prev_ms));
		}
		prev_ms = SDL_GetTicks();


		/// --------- Refresh screen
		if(screen_refresh){
		  menu_screen_refresh(screen, menuItem, prevItem, scroll, menu_confirmation, 0);
		}

		/// --------- reset screen refresh ---------
		screen_refresh = 0;
	}

	/// ------ Restore last keymap ------
	system(SHELL_CMD_KEYMAP_RESUME);

	/// ------ Reset prev key repeat params -------
	if(SDL_EnableKeyRepeat(backup_key_repeat_delay, backup_key_repeat_interval)){
		MENU_ERROR_PRINTF("ERROR with SDL_EnableKeyRepeat: %s\n", SDL_GetError());
	}

	/// --------- Clear HW screen ----------
	if(SDL_BlitSurface(background_screen, NULL, screen, NULL)){
		MENU_ERROR_PRINTF("ERROR Could not Clear screen: %s\n", SDL_GetError());
	}

	/// --------- Flip Screen ----------
	SDL_Flip(screen);

	if(background_screen != NULL){
		SDL_FreeSurface(background_screen);
		background_screen = NULL;
	}
	MENU_DEBUG_PRINTF("Leave Menu\n");
	if (returnCode != MENU_RETURN_MENU)
		plat_video_menu_leave();
	return returnCode;
}

#ifdef HAS_MENU_ASPECT_RATIO
void FK_NextAspectRatio(void)
{
	char shell_cmd[100];
	FILE *fp;

	read_aspect_ratio();
	menu_aspect_ratio = (menu_aspect_ratio+1)%NB_ASPECT_RATIOS_TYPES;
	update_aspect_ratio();
	scale_update_scaler();
	plat_video_menu_leave();
	snprintf(shell_cmd, 100, "%s %d \"    DISPLAY MODE: %s\"",
    SHELL_CMD_NOTIF_SET, NOTIF_SECONDS_DISP, aspect_ratio_name[menu_aspect_ratio]);
	fp = popen(shell_cmd, "r");
	if (fp == NULL) {
		printf("Failed to run command %s\n", shell_cmd);
	} else {
		pclose(fp);
	}
}
#endif

/****************************/
/*    Quick Resume Menu     */
/****************************/
int FK_RunResumeMenu(SDL_Surface *hw_screen)
{
    MENU_DEBUG_PRINTF("Init resume menu\n");

    /* Decare vars */
    SDL_Surface *text_surface = NULL;
    char text_tmp[40];
    SDL_Rect text_pos;
    SDL_Event event;
    uint32_t prev_ms = SDL_GetTicks();
    uint32_t cur_ms = SDL_GetTicks();
    stop_menu_loop = 0;
    uint8_t screen_refresh = 1;
    uint8_t menu_confirmation = 0;
    int option_idx=RESUME_YES;

    /* Save prev key repeat params and set new Key repeat */
    SDL_GetKeyRepeat(&backup_key_repeat_delay, &backup_key_repeat_interval);
    if(SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL)){
        MENU_ERROR_PRINTF("ERROR with SDL_EnableKeyRepeat: %s\n", SDL_GetError());
    }

    /* Load BG */
    SDL_Surface *img_square_bg = IMG_Load(MENU_PNG_BG_PATH);
    if(!img_square_bg) {
        MENU_ERROR_PRINTF("ERROR IMG_Load: %s\n", IMG_GetError());
    }
    SDL_Surface *bg_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, hw_screen->w, hw_screen->h, 16, 0, 0, 0, 0);
    SDL_BlitSurface(img_square_bg, NULL, bg_surface, NULL);
    SDL_FreeSurface(img_square_bg);


    /*  Print top arrow */
    SDL_Rect pos_arrow_top;
    pos_arrow_top.x = (bg_surface->w - img_arrow_top->w)/2;
    pos_arrow_top.y = (bg_surface->h - MENU_BG_SQUARE_HEIGHT)/4 - img_arrow_top->h/2;
    SDL_BlitSurface(img_arrow_top, NULL, bg_surface, &pos_arrow_top);

    /*  Print bottom arrow */
    SDL_Rect pos_arrow_bottom;
    pos_arrow_bottom.x = (bg_surface->w - img_arrow_bottom->w)/2;
    pos_arrow_bottom.y = bg_surface->h -
            (bg_surface->h - MENU_BG_SQUARE_HEIGHT)/4 - img_arrow_bottom->h/2;
    SDL_BlitSurface(img_arrow_bottom, NULL, bg_surface, &pos_arrow_bottom);

    if (text_surface)
        SDL_FreeSurface(text_surface);

    /* Main loop */
    while (!stop_menu_loop)
    {
        /* Handle keyboard events */
        while (SDL_PollEvent(&event))
        switch(event.type)
        {
            case SDL_QUIT:
                stop_menu_loop = 1;
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_b:
                        if(menu_confirmation){
                            /// ------ Reset menu confirmation ------
                            menu_confirmation = 0;

                            /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        /*else{
                            stop_menu_loop = 1;
                        }*/
                        break;

                    case SDLK_q:
                    case SDLK_ESCAPE:
                        /*stop_menu_loop = 1;*/
                        break;

                    case SDLK_u:
                    case SDLK_UP:
                        MENU_DEBUG_PRINTF("Option UP\n");
                        option_idx = (!option_idx)?(NB_RESUME_OPTIONS-1):(option_idx-1);

                        /// ------ Reset menu confirmation ------
                        menu_confirmation = 0;

                        /// ------ Refresh screen ------
                        screen_refresh = 1;
                        break;

                    case SDLK_d:
                    case SDLK_DOWN:
                        MENU_DEBUG_PRINTF("Option DOWN\n");
                        option_idx = (option_idx+1)%NB_RESUME_OPTIONS;

                        /// ------ Reset menu confirmation ------
                        menu_confirmation = 0;

                        /// ------ Refresh screen ------
                        screen_refresh = 1;
                        break;

                    case SDLK_a:
                    case SDLK_RETURN:
                        MENU_DEBUG_PRINTF("Pressed A\n");
                        if(menu_confirmation){
                            MENU_DEBUG_PRINTF("Confirmed\n");

                            /// ----- exit menu  ----
                            stop_menu_loop = 1;
                        }
                        else{
                            MENU_DEBUG_PRINTF("Asking confirmation\n");
                            menu_confirmation = 1;

                            /// ------ Refresh screen ------
                            screen_refresh = 1;
                        }
                        break;

                    default:
                        //MENU_DEBUG_PRINTF("Keydown: %d\n", event.key.keysym.sym);
                        break;
            }
            break;
        }

        /* Handle FPS */
        cur_ms = SDL_GetTicks();
        if(cur_ms-prev_ms < 1000/FPS_MENU){
            SDL_Delay(1000/FPS_MENU - (cur_ms-prev_ms));
        }
        prev_ms = SDL_GetTicks();

        /* Refresh screen */
        if(screen_refresh){
            /* Clear and draw BG */
            SDL_FillRect(hw_screen, NULL, 0);
            if(SDL_BlitSurface(bg_surface, NULL, hw_screen, NULL)){
                MENU_ERROR_PRINTF("ERROR Could not draw background: %s\n", SDL_GetError());
            }

            /* Draw resume or reset option */
            text_surface = TTF_RenderText_Blended(menu_title_font, resume_options_str[option_idx], text_color);
            text_pos.x = (hw_screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
            text_pos.y = hw_screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2;
            SDL_BlitSurface(text_surface, NULL, hw_screen, &text_pos);

            /* Draw confirmation */
            if(menu_confirmation){
                sprintf(text_tmp, "Are you sure ?");
                text_surface = TTF_RenderText_Blended(menu_info_font, text_tmp, text_color);
                text_pos.x = (hw_screen->w - MENU_ZONE_WIDTH)/2 + (MENU_ZONE_WIDTH - text_surface->w)/2;
                text_pos.y = hw_screen->h - MENU_ZONE_HEIGHT/2 - text_surface->h/2 + 2*padding_y_from_center_menu_zone;
                SDL_BlitSurface(text_surface, NULL, hw_screen, &text_pos);
            }

            /* Flip Screen */
            SDL_Flip(hw_screen);
        }

        /* reset screen refresh */
        screen_refresh = 0;
    }

    /* Free SDL Surfaces */
    if(bg_surface)
        SDL_FreeSurface(bg_surface);
    if(text_surface)
        SDL_FreeSurface(text_surface);

    /* Reset prev key repeat params */
    if(SDL_EnableKeyRepeat(backup_key_repeat_delay, backup_key_repeat_interval)){
        MENU_ERROR_PRINTF("ERROR with SDL_EnableKeyRepeat: %s\n", SDL_GetError());
    }

    return option_idx;
}
