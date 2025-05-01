# Global definitions
platform   ?= unix
core_platform ?= $(platform)

CC        = $(CROSS_COMPILE)gcc
SYSROOT   = $(shell $(CC) --print-sysroot)

PROCS     = -j4

SOURCES   = libpicofe/input.c libpicofe/in_sdl.c libpicofe/linux/in_evdev.c libpicofe/linux/plat.c libpicofe/fonts.c libpicofe/readpng.c libpicofe/config_file.c cheat.c config.c content.c core.c menu.c main.c options.c overrides.c patch.c scale.c unzip.c util.c video.c

BIN       = picoarch

unexport CFLAGS
CFLAGS     += -Wall
CFLAGS     += -fdata-sections -ffunction-sections -DPICO_HOME_DIR='"/.picoarch/"' -flto
CFLAGS     += -I./ -I./libretro-common/include/ $(shell $(SYSROOT)/usr/bin/sdl-config --cflags)

LDFLAGS    = -lc -ldl -lgcc -lm -lSDL -lasound -lpng -lz -Wl,--gc-sections -flto

# Unpolished or slow cores that build
# EXTRA_CORES += mame2003_plus prboom scummvm tyrquake

CORES = beetle-pce-fast bluemsx fceumm fmsx gambatte gme gpsp mame2000 mednafen_ngp mednafen_wswan pcsx_rearmed picodrive pokemini quicknes smsplus-gx snes9x2002 snes9x2005 stella2014 $(EXTRA_CORES)

ifeq ($(device), picolyra)
	CORES = beetle-pce-fast bluemsx fceumm fmsx gambatte gme mame2000 quicknes smsplus-gx snes9x2002 snes9x2005 stella2014 $(EXTRA_CORES)
endif

ifneq ($(platform), trimui)
	ifneq ($(device), picolyra)
 		CORES := $(CORES) dosbox-pure fake-08 fbalpha2012 snes9x2005_plus snes9x2010
	endif
endif

# CORES = dosbox-pure

beetle-pce-fast_REPO = https://github.com/libretro/beetle-pce-fast-libretro
beetle-pce-fast_CORE = mednafen_pce_fast_libretro.so
beetle-pce-fast_TYPES = pce,cue,ccd,chd,toc,m3u

bluemsx_REPO = https://github.com/libretro/blueMSX-libretro
bluemsx_TYPES = rom,ri,mx1,mx2,dsk,col,sg,sc,cas,m3u

dosbox-pure_REPO = https://github.com/schellingb/dosbox-pure
dosbox-pure_CORE = dosbox_pure_libretro.so
dosbox-pure_TYPES = zip,dosz,exe,com,bat,iso,cue,ins,img,ima,vhd,jrc,tc,m3u,m3u8,conf
dosbox-pure_FLAGS = STRIPCMD="$(CROSS_COMPILE)strip"
ifeq ($(platform), funkey-s)
dosbox-pure_FLAGS += CYCLE_LIMIT=8200
endif


fake-08_REPO = https://github.com/jtothebell/fake-08
fake-08_BUILD_PATH = fake-08/platform/libretro
fake-08_MAKEFILE = Makefile
fake-08_CORE = fake08_libretro.so
fake-08_TYPES = p8,png

fbalpha2012_BUILD_PATH = fbalpha2012/svn-current/trunk
fbalpha2012_MAKEFILE = makefile.libretro
fbalpha2012_TYPES = zip

fceumm_REPO = https://github.com/libretro/libretro-fceumm
fceumm_MAKEFILE = Makefile.libretro
fceumm_TYPES = fds,nes,unf,unif

fmsx_REPO = https://github.com/libretro/fmsx-libretro
fmsx_TYPES = rom,mx1,mx2,dsk,cas

gambatte_REPO = https://github.com/libretro/gambatte-libretro
gambatte_TYPES = gb,gbc,dmg,zip

gme_REPO = https://github.com/libretro/libretro-gme

gpsp_TYPES = gba,bin,zip

mame2000_REPO = https://github.com/libretro/mame2000-libretro
mame2000_TYPES = zip

mame2003_plus_REPO = https://github.com/libretro/mame2003-plus-libretro
mame2003_plus_TYPES = zip

mednafen_ngp_REPO = https://github.com/libretro/beetle-ngp-libretro
mednafen_ngp_TYPES = ngp,ngc,ngpc,npc

mednafen_wswan_REPO = https://github.com/libretro/beetle-wswan-libretro
mednafen_wswan_TYPES = ws,wsc,pc2

pcsx_rearmed_MAKEFILE = Makefile.libretro
pcsx_rearmed_TYPES = bin,cue,img,mdf,pbp,toc,cbn,m3u,chd

picodrive_MAKEFILE = Makefile.libretro
picodrive_TYPES = bin,gen,smd,md,32x,cue,iso,chd,sms,gg,m3u,68k,sgd

pokemini_TYPES = min

prboom_REPO = https://github.com/libretro/libretro-prboom
prboom_TYPES = wad,iwad,pwad,lmp

quicknes_REPO = https://github.com/libretro/QuickNES_Core
quicknes_TYPES = nes

scummvm_TYPES = scummvm

smsplus-gx_MAKEFILE = Makefile.libretro
smsplus-gx_CORE = smsplus_libretro.so
smsplus-gx_TYPES = sms,bin,rom,col,gg,sg

snes9x2002_TYPES = smc,fig,sfc,gd3,gd7,dx2,bsx,swc,zip

snes9x2005_REPO = https://git.crowdedwood.com/snes9x2005
snes9x2005_REVISION = performance
snes9x2005_TYPES = smc,fig,sfc,gd3,gd7,dx2,bsx,swc,zip

snes9x2005_plus_REPO = https://git.crowdedwood.com/snes9x2005
snes9x2005_plus_REVISION = performance
snes9x2005_plus_FLAGS = USE_BLARGG_APU=1
snes9x2005_plus_TYPES = smc,fig,sfc,gd3,gd7,dx2,bsx,swc,zip

snes9x2010_TYPES = smc,fig,sfc,gd3,gd7,dx2,bsx,swc,zip

stella2014_REPO = https://github.com/libretro/stella2014-libretro
stella2014_TYPES = a26,bin

tyrquake_TYPES = pak

ifeq ($(platform), trimui)
	SOURCES += plat_trimui.c
	CFLAGS += -mcpu=arm926ej-s -mtune=arm926ej-s -fno-PIC -DCONTENT_DIR='"/mnt/SDCARD/Roms"'
	LDFLAGS += -fno-PIC
else ifeq ($(platform), funkey-s)
	SOURCES += plat_funkey.c funkey/fk_menu.c funkey/fk_instant_play.c
	CFLAGS += -DCONTENT_DIR='"/mnt"' -DFUNKEY_S
	LDFLAGS += -fPIC
	LDFLAGS += -lSDL_image -lSDL_ttf # For fk_menu
	core_platform = unix-armv7-hardfloat-neon
else ifeq ($(platform), unix)
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

ifeq ($(MINUI), 1)
	MMENU = 1
	CFLAGS += -DMINUI
endif

ifeq ($(MMENU), 1)
	CFLAGS += -DMMENU
	LDFLAGS += -lSDL_image -lSDL_ttf -ldl
endif

CFLAGS += $(EXTRA_CFLAGS)

SOFILES = $(foreach core,$(CORES),$(core)_libretro.so)

.PHONY: print-%
print-%:
	@echo '$*=$($*)'

.PHONY: all
all: $(BIN) cores

libpicofe/.patched:
	cd libpicofe && ($(foreach patch, $(sort $(wildcard patches/libpicofe/*.patch)), patch --no-backup-if-mismatch --merge -p1 < ../$(patch) &&) touch .patched)

reverse = $(if $(wordlist 2,2,$(1)),$(call reverse,$(wordlist 2,$(words $(1)),$(1))) $(firstword $(1)),$(1))

.PHONY: clean-libpicofe
clean-libpicofe:
	test ! -f libpicofe/.patched || (cd libpicofe && ($(foreach patch, $(call reverse,$(sort $(wildcard patches/libpicofe/*.patch))), patch -R --merge --no-backup-if-mismatch -p1 < ../$(patch) &&) rm .patched))

CFLAGS += -MMD -MP
DEPS=$(SOURCES:.c=.d)
$(DEPS):

include $(wildcard $(DEPS))

OBJS = $(SOURCES:.c=.o)

$(BIN): libpicofe/.patched $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(BIN)

define CORE_template =

$1_REPO ?= https://github.com/libretro/$(1)/

$1_BUILD_PATH ?= $(1)

$1_MAKE = make $(and $($1_MAKEFILE),-f $($1_MAKEFILE)) platform=$(core_platform) $(and $(DEBUG),DEBUG=$(DEBUG)) $(and $(PROFILE),PROFILE=$(PROFILE)) CC=$(CC) CXX=$(CXX) $($(1)_FLAGS)

$(1):
	git clone $(if $($1_REVISION),,--depth 1) --recursive $$($(1)_REPO) $(1)
	$(if $1_REVISION,cd $(1) && git checkout $($1_REVISION),)
	(test ! -d patches/$(1)) || (cd $(1) && $(foreach patch, $(sort $(wildcard patches/$(1)/*.patch)), patch --merge --no-backup-if-mismatch -p1 < ../$(patch) &&) true)

$(1)/$(1)_libretro.so: $(1)
	cd $$($1_BUILD_PATH) && $$($1_MAKE) $(PROCS)

$(1)_libretro.so: $(1)/$(1)_libretro.so
	cp $$($1_BUILD_PATH)/$(if $($(1)_CORE),$($(1)_CORE),$(1)_libretro.so) $(1)_libretro.so

clean-$(1):
	test ! -d $(1) || cd $$($1_BUILD_PATH) && $$($1_MAKE) clean
	rm -f $(1)_libretro.so
endef

$(foreach core,$(CORES),$(eval $(call CORE_template,$(core))))

.PHONY: cores
cores: $(SOFILES)

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

.PHONY: distclean
distclean: clean
	rm -rf $(CORES) *.zip

ifeq ($(platform), trimui)

beetle-pce-fast_NAME = pce_fast
beetle-pce-fast_ROM_DIR = PCE
beetle-pce-fast_PAK_NAME = TurboGrafx-16

bluemsx_NAME = blueMSX
bluemsx_ROM_DIR = MSX
bluemsx_PAK_NAME = MSX (blueMSX)

fbalpha2012_NAME = fba2012
fbalpha2012_ROM_DIR = ARCADE
fbalpha2012_PAK_NAME = Arcade (FBA)

fceumm_ROM_DIR = FC
fceumm_PAK_NAME = Nintendo (fceumm)

fmsx_NAME = fMSX
fmsx_ROM_DIR = MSX
fmsx_PAK_NAME = MSX

gambatte_ROM_DIR = GB
gambatte_PAK_NAME = Game Boy

gpsp_ROM_DIR = GBA
gpsp_PAK_NAME = Game Boy Advance
define gpsp_PAK_EXTRA

needs-swap

endef

gme_ROM_DIR = MUSIC
gme_PAK_NAME = Game Music

mame2000_ROM_DIR = ARCADE
mame2000_PAK_NAME = Arcade

mame2003_plus_NAME = mame2003+
mame2003_plus_ROM_DIR = ARCADE
mame2003_plus_PAK_NAME = Arcade (MAME 2003-plus)

mednafen_ngp_NAME = ngp
mednafen_ngp_ROM_DIR = NGP
mednafen_ngp_PAK_NAME = Neo Geo Pocket

mednafen_wswan_NAME = wswan
mednafen_wswan_ROM_DIR = WS
mednafen_wswan_PAK_NAME = WonderSwan

picodrive_ROM_DIR = MD
picodrive_PAK_NAME = Genesis

pokemini_ROM_DIR = POKEMINI
pokemini_PAK_NAME = PokeMini

pcsx_rearmed_ROM_DIR = PS
pcsx_rearmed_PAK_NAME = PlayStation
define pcsx_rearmed_PAK_EXTRA

needs-swap

endef

quicknes_ROM_DIR = FC
quicknes_PAK_NAME = Nintendo

smsplus-gx_ROM_DIR = MS
smsplus-gx_PAK_NAME = Game Gear

snes9x2002_ROM_DIR = SFC
snes9x2002_PAK_NAME = Super Nintendo

snes9x2005_ROM_DIR = SFC
snes9x2005_PAK_NAME = Super Nintendo (2005)

stella2014_ROM_DIR = 2600
stella2014_PAK_NAME = Atari 2600

# -- gmenunx

dist-gmenu-section:
	mkdir -p pkg/gmenunx/Apps/picoarch
	mkdir -p pkg/gmenunx/Apps/gmenunx/sections/emulators
	mkdir -p pkg/gmenunx/Apps/gmenunx/sections/libretro
	touch pkg/gmenunx/Apps/gmenunx/sections/libretro/.section

dist-gmenu-picoarch: $(BIN) dist-gmenu-section
	cp $(BIN) pkg/gmenunx/Apps/picoarch
	$(file >pkg/gmenunx/Apps/gmenunx/sections/emulators/picoarch,$(picoarch_SHORTCUT))

define CORE_gmenushortcut =

$1_NAME ?= $1

define $1_SHORTCUT
title=$$($1_NAME)
exec=/mnt/SDCARD/Apps/picoarch/picoarch
params=/mnt/SDCARD/Apps/picoarch/$1_libretro.so
selectordir=/mnt/SDCARD/Roms/$($1_ROM_DIR)
selectorfilter=$($1_TYPES)
endef

dist-gmenu-$(1): $(BIN) $(1)_libretro.so dist-gmenu-picoarch dist-gmenu-section
	cp $1_libretro.so pkg/gmenunx/Apps/picoarch
	$$(file >pkg/gmenunx/Apps/gmenunx/sections/libretro/$(1),$$($(1)_SHORTCUT))

endef

$(foreach core, $(CORES),$(eval $(call CORE_gmenushortcut,$(core))))

define picoarch_SHORTCUT
title=$(BIN)
exec=/mnt/SDCARD/Apps/picoarch/picoarch
endef

dist-gmenu: $(foreach core, $(CORES), dist-gmenu-$(core)) dist-gmenu-picoarch
	cp README.trimui.md pkg/

# -- MinUI

ifeq ($(MINUI), 1)
define CORE_pak_template =

define $1_LAUNCH_SH
#!/bin/sh
# $($1_PAK_NAME).pak/launch.sh

EMU_EXE=picoarch
EMU_DIR=$$$$(dirname "$$$$0")
ROM_DIR=$$$${EMU_DIR/.pak/}
ROM_DIR=$$$${ROM_DIR/Emus/Roms}
EMU_NAME=$$$${ROM_DIR/\/mnt\/SDCARD\/Roms\//}
ROM=$$$${1}
$($1_PAK_EXTRA)
HOME="$$$$ROM_DIR"
cd "$$$$EMU_DIR"
"$$$$EMU_DIR/$$$$EMU_EXE" ./$1_libretro.so "$$$$ROM" &> "/mnt/SDCARD/.minui/logs/$$$$EMU_NAME.txt"
endef

dist-minui-$(1): $(BIN) $(1)_libretro.so
	mkdir -p "pkg/MinUI/Emus/$($1_PAK_NAME).pak"
	$$(file >$1_launch.sh,$$($1_LAUNCH_SH))
	mv $1_launch.sh "pkg/MinUI/Emus/$($1_PAK_NAME).pak/launch.sh"
	cp $(BIN) $1_libretro.so "pkg/MinUI/Emus/$($1_PAK_NAME).pak"

endef

define picoarch_LAUNCH_SH
#!/bin/sh
# picoarch.pak/launch.sh

EMU_EXE=picoarch
EMU_DIR=$$(dirname "$$0")
EMU_NAME=$$EMU_EXE

needs-swap

HOME="/mnt/SDCARD/Games/picoarch.pak/"
cd "$$EMU_DIR"
"$$EMU_DIR/$$EMU_EXE" &> "/mnt/SDCARD/.minui/logs/$$EMU_NAME.txt"
endef

dist-minui-picoarch: $(BIN) cores
	mkdir -p "pkg/MinUI/Games/picoarch.pak"
	$(file >picoarch_launch.sh,$(picoarch_LAUNCH_SH))
	mv picoarch_launch.sh "pkg/MinUI/Games/picoarch.pak/launch.sh"
	cp $(BIN) $(SOFILES) "pkg/MinUI/Games/picoarch.pak"

$(foreach core, $(CORES),$(eval $(call CORE_pak_template,$(core))))

dist-minui: $(foreach core, $(CORES), dist-minui-$(core)) dist-minui-picoarch
	cp README.trimui.md pkg/

endif # MINUI=1

picoarch.zip:
	make platform=trimui PROFILE=APPLY clean-all dist-gmenu
	rm -f $(OBJS) $(BIN)
	make platform=trimui PROFILE=APPLY EXTRA_CFLAGS=-Wno-error=coverage-mismatch MINUI=1 dist-minui
	cd pkg && zip -r ../picoarch.zip *

endif # platform=trimui

ifeq ($(platform), funkey-s)

beetle-pce-fast_NAME = pce_fast
beetle-pce-fast_ROM_DIR = /mnt/PCE-TurboGrafx
beetle-pce-fast_ICON_URL = https://raw.githubusercontent.com/FunKey-Project/FunKey-OS/master/FunKey/package/mednafen/opk/pce/pce.png
beetle-pce-fast_ICON = pce

bluemsx_NAME = blueMSX
bluemsx_ROM_DIR = /mnt/MSX
bluemsx_ICON_URL = https://raw.githubusercontent.com/MiyooCFW/gmenu2x/gmenunx/assets/miyoo/skins/PixUI/icons/dingux-msx.png
bluemsx_ICON = dingux-msx

dosbox-pure_ROM_DIR = /mnt/DOS
dosbox-pure_ICON_URL = https://raw.githubusercontent.com/MiyooCFW/gmenu2x/gmenunx/assets/miyoo/skins/PixUI/icons/dosbox.png
dosbox-pure_ICON = dosbox

fake-08_NAME = fake-08
fake-08_ROM_DIR = /mnt/PICO-8
fake-08_ICON_URL = https://raw.githubusercontent.com/jtothebell/fake-08/master/platform/vita/sce_sys/icon0.png
fake-08_ICON = icon0

fbalpha2012_NAME = fba2012
fbalpha2012_ROM_DIR = /mnt/Arcade
fbalpha2012_ICON_URL = https://raw.githubusercontent.com/MiyooCFW/gmenu2x/gmenunx/assets/miyoo/skins/PixUI/icons/fba.png
fbalpha2012_ICON = fba

fceumm_ROM_DIR = /mnt/NES
fceumm_ICON_URL = https://raw.githubusercontent.com/FunKey-Project/FunKey-OS/master/FunKey/package/FCEUX/opk/nes/nes.png
fceumm_ICON = nes

fmsx_NAME = fMSX
fmsx_ROM_DIR = /mnt/MSX
fmsx_ICON_URL = https://raw.githubusercontent.com/MiyooCFW/gmenu2x/gmenunx/assets/miyoo/skins/PixUI/icons/dingux-msx.png
fmsx_ICON = dingux-msx

gambatte_ROM_DIR = /mnt/Game Boy
gambatte_ICON_URL = https://raw.githubusercontent.com/FunKey-Project/FunKey-OS/master/FunKey/package/gnuboy/opk/gb/gb.png
gambatte_ICON = gb

gme_ROM_DIR = /mnt/Music
gme_TYPES = ay,gbs,gym,hes,kss,nsf,nsfe,sap,spc,vgm,vgz,zip
gme_ICON_URL = https://raw.githubusercontent.com/MiyooCFW/gmenu2x/gmenunx/assets/miyoo/skins/PixUI/icons/gmu.png
gme_ICON = gmu

gpsp_ROM_DIR = /mnt/Game Boy Advance
gpsp_ICON_URL = https://raw.githubusercontent.com/FunKey-Project/FunKey-OS/master/FunKey/package/gpsp/opk/gba/gba.png
gpsp_ICON = gba

mame2000_ROM_DIR = /mnt/Arcade
mame2000_ICON_URL = https://raw.githubusercontent.com/MiyooCFW/gmenu2x/gmenunx/assets/miyoo/skins/PixUI/icons/mame.png
mame2000_ICON = mame

mame2003_plus_NAME = mame2003+
mame2003_plus_ROM_DIR = /mnt/Arcade
mame2003_plus_ICON_URL = https://raw.githubusercontent.com/MiyooCFW/gmenu2x/gmenunx/assets/miyoo/skins/PixUI/icons/mame.png
mame2003_plus_ICON = icon

mednafen_ngp_NAME = ngp
mednafen_ngp_ROM_DIR = /mnt/Neo Geo Pocket
mednafen_ngp_ICON_URL = https://raw.githubusercontent.com/FunKey-Project/FunKey-OS/master/FunKey/package/mednafen/opk/ngp/ngp.png
mednafen_ngp_ICON = ngp

mednafen_wswan_NAME = wswan
mednafen_wswan_ROM_DIR = /mnt/WonderSwan
mednafen_wswan_ICON_URL = https://raw.githubusercontent.com/FunKey-Project/FunKey-OS/master/FunKey/package/mednafen/opk/wonderswan/wonderswan.png
mednafen_wswan_ICON = wonderswan

pcsx_rearmed_ROM_DIR = /mnt/PS1
pcsx_rearmed_ICON_URL = https://raw.githubusercontent.com/MiyooCFW/gmenu2x/gmenunx/assets/miyoo/skins/PixUI/icons/pcsx4all.png
pcsx_rearmed_ICON = pcsx4all

picodrive_ROM_DIR = /mnt/Sega Genesis
picodrive_ICON_URL = https://raw.githubusercontent.com/FunKey-Project/FunKey-OS/master/FunKey/package/picodrive/opk/megadrive/megadrive.png
picodrive_ICON = megadrive

pokemini_ROM_DIR = /mnt/PokeMini
pokemini_ICON_URL = https://raw.githubusercontent.com/MiyooCFW/gmenu2x/gmenunx/assets/miyoo/skins/PixUI/icons/pokemini.png
pokemini_ICON = pokemini

quicknes_ROM_DIR = /mnt/NES
quicknes_ICON_URL = https://raw.githubusercontent.com/FunKey-Project/FunKey-OS/master/FunKey/package/FCEUX/opk/nes/nes.png
quicknes_ICON = nes

smsplus-gx_ROM_DIR = /mnt/Game Gear
smsplus-gx_ICON_URL = https://raw.githubusercontent.com/FunKey-Project/FunKey-OS/master/FunKey/package/mednafen/opk/gamegear/gamegear.png
smsplus-gx_ICON = gamegear

snes9x2002_ROM_DIR = /mnt/SNES
snes9x2002_ICON_URL = https://raw.githubusercontent.com/FunKey-Project/FunKey-OS/master/FunKey/package/PocketSNES/opk/snes/snes.png
snes9x2002_ICON = snes

snes9x2005_ROM_DIR = /mnt/SNES
snes9x2005_ICON_URL = https://raw.githubusercontent.com/FunKey-Project/FunKey-OS/master/FunKey/package/PocketSNES/opk/snes/snes.png
snes9x2005_ICON = snes

snes9x2005_plus_NAME = snes9x2005+
snes9x2005_plus_ICON_URL = https://raw.githubusercontent.com/FunKey-Project/FunKey-OS/master/FunKey/package/PocketSNES/opk/snes/snes.png
snes9x2005_plus_ICON = snes
snes9x2005_plus_ROM_DIR = /mnt/SNES

snes9x2010_NAME = snes9x2010
snes9x2010_ICON_URL = https://raw.githubusercontent.com/FunKey-Project/FunKey-OS/master/FunKey/package/PocketSNES/opk/snes/snes.png
snes9x2010_ICON = snes
snes9x2010_ROM_DIR = /mnt/SNES

stella2014_NAME = stella2014
stella2014_ICON_URL = https://raw.githubusercontent.com/MiyooCFW/gmenu2x/gmenunx/assets/miyoo/skins/PixUI/icons/stella-od.png
stella2014_ICON = stella-od
stella2014_ROM_DIR = /mnt/Atari 2600

define CORE_opk =

$1_NAME ?= $1

define $1_DESKTOP
[Desktop Entry]
Name=$$($1_NAME)
Comment=
Exec=picoarch ./$1_libretro.so %f
Icon=$$($1_ICON)
SelectorBrowser=true
SelectorDir=$($1_ROM_DIR)
SelectorFilter=$($1_TYPES),zip
Terminal=false
Type=Application
StartupNotify=true
Categories=emulators;
endef

picoarch-$(1).opk: $(BIN) $(1)_libretro.so
	mkdir -p .opkdata
	cp $1_libretro.so .opkdata
	$$(file >$$($(1)_NAME).funkey-s.desktop,$$($(1)_DESKTOP))
	mv $$($(1)_NAME).funkey-s.desktop .opkdata
	cp $(BIN) $(1)_libretro.so .opkdata
	$(if $($(1)_ICON_URL),cd .opkdata && curl -L $($(1)_ICON_URL) -O && mogrify -resize '32x32>' $($(1)_ICON).png,)
	cd .opkdata && mksquashfs * ../$$@ -all-root -no-xattrs -noappend -no-exports
	rm -r .opkdata
endef

$(foreach core, $(CORES),$(eval $(call CORE_opk,$(core))))

define picoarch_DESKTOP
[Desktop Entry]
Name=picoarch
Comment=Small screen libretro frontend
Exec=picoarch
Icon=sdlretro_icon
Terminal=false
Type=Application
StartupNotify=true
Categories=emulators;
endef

picoarch.opk: $(BIN) $(SOFILES)
	mkdir -p .opkdata
	$(file >picoarch.funkey-s.desktop,$(picoarch_DESKTOP))
	mv picoarch.funkey-s.desktop .opkdata
	cp $(BIN) $(SOFILES) .opkdata
	cd .opkdata && curl -L -O https://raw.githubusercontent.com/FunKey-Project/sdlretro/master/data/sdlretro_icon.png
	cd .opkdata && mksquashfs * ../$@ -all-root -no-xattrs -noappend -no-exports
	rm -r .opkdata

define picoarch_lite_DESKTOP
[Desktop Entry]
Name=picoarch-lite
Comment=Small screen libretro frontend
Exec=picoarch %f
Icon=sdlretro_icon
SelectorBrowser=true
SelectorDir=/mnt/Libretro/cores
SelectorFilter=so
Terminal=false
Type=Application
StartupNotify=true
Categories=emulators;
endef

picoarch-lite.opk: $(BIN)
	mkdir -p .opkdata
	$(file >picoarch.funkey-s.desktop,$(picoarch_lite_DESKTOP))
	mv picoarch.funkey-s.desktop .opkdata
	cp $(BIN) .opkdata
	cd .opkdata && curl -L -O https://raw.githubusercontent.com/FunKey-Project/sdlretro/master/data/sdlretro_icon.png
	cd .opkdata && mksquashfs * ../$@ -all-root -no-xattrs -noappend -no-exports
	rm -r .opkdata

picoarch-funkey-s.zip: picoarch.opk $(foreach core, $(CORES), picoarch-$(core).opk)
	rm -f $@
	zip $@ $^

cores-funkey-s.zip: $(SOFILES)
	rm -f $@
	zip $@ $^

.PHONY: dist
dist: picoarch-lite.opk picoarch-funkey-s.zip cores-funkey-s.zip

endif # platform=funkey-s
