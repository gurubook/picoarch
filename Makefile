# Global definitions
platform   ?= unix
core_platform ?= $(platform)

# CC        = $(CROSS_COMPILE)gcc
# SYSROOT   = $(shell $(CC) --print-sysroot)

PROCS     = -j4

SOURCES   = libpicofe/input.c libpicofe/in_sdl.c libpicofe/linux/in_evdev.c libpicofe/linux/plat.c libpicofe/fonts.c libpicofe/readpng.c libpicofe/config_file.c cheat.c config.c content.c core.c menu.c main.c options.c overrides.c patch.c scale.c unzip.c util.c video.c

BIN       = picoarch

CFLAGS     += -fdata-sections -ffunction-sections -DPICO_HOME_DIR='"/.picoarch/"' -flto
CFLAGS     += -I./ -I./libretro-common/include/ $(shell $(SYSROOT)/usr/bin/sdl-config --cflags) 
#$(SDL_CFLAGS)

LDFLAGS    = -lc -ldl -lgcc -lm -lSDL -lasound -lpng -lz -Wl,--gc-sections -flto

SOURCES += plat_linux.c
LDFLAGS += -fPIE

ifeq ($(platform), unix)
	SOURCES += plat_linux.c
	LDFLAGS += -fPIE
endif

ifeq ($(DEBUG), 1)
	CFLAGS += -Og -g
	LDFLAGS += -g
else
	CFLAGS += -Ofast -DNDEBUG

ifneq ($(PROFILE), 1)
	LDFLAGS += -s
endif

endif

ifeq ($(PROFILE), 1)
	CFLAGS += -fno-omit-frame-pointer -pg -g
	LDFLAGS += -pg -g
else ifeq ($(PROFILE), GENERATE)
	CFLAGS	+= -fprofile-generate=./profile/picoarch
	LDFLAGS	+= -lgcov
else ifeq ($(PROFILE), APPLY)
	CFLAGS	+= -fprofile-use -fprofile-dir=./profile/picoarch -fbranch-probabilities
endif

ifeq ($(MMENU), 1)
	CFLAGS += -DMMENU
	LDFLAGS += -lSDL_image -lSDL_ttf -ldl
endif

CFLAGS += $(EXTRA_CFLAGS)

libpicofe/.patched:
	cd libpicofe && ($(foreach patch, $(sort $(wildcard patches/libpicofe/*.patch)), patch --no-backup-if-mismatch --merge -p1 < ../$(patch) &&) touch .patched)

reverse = $(if $(wordlist 2,2,$(1)),$(call reverse,$(wordlist 2,$(words $(1)),$(1))) $(firstword $(1)),$(1))

.PHONY: clean-libpicofe
clean-libpicofe:
	test ! -f libpicofe/.patched || (cd libpicofe && ($(foreach patch, $(call reverse,$(sort $(wildcard patches/libpicofe/*.patch))), patch -R --merge --no-backup-if-mismatch -p1 < ../$(patch) &&) rm .patched))

CCFLAGS += -MMD -MP
DEPS=$(SOURCES:.c=.d)
$(DEPS):

include $(wildcard $(DEPS))

OBJS = $(SOURCES:.c=.o)

$(BIN): libpicofe/.patched $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(BIN)
	
.PHONY: clean-picoarch
clean-picoarch:
	rm -f $(DEPS) $(OBJS) $(BIN)
	rm -rf pkg
	rm -f *.opk

.PHONY: clean
clean: clean-libpicofe clean-picoarch
	rm -f $(SOFILES)

.PHONY: clean-all
clean-all: $(foreach core,$(CORES),clean-$(core)) clean


