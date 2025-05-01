# picoarch - a libretro frontend designed for small screens and low power

picoarch uses libpicofe and SDL to create a small frontend to libretro cores. It's designed for small (320x240 2.0-2.4") screen, low-powered devices like the Trimui Model S (PowKiddy A66) and FunKey S.

## Running

picoarch can be run by specifying the core library and the content to run:

```
./picoarch /path/to/core_name_libretro.so /path/to/game.gba
```

If you do not specify core or content, picoarch will have you select a core from the current directory and content using the built-in file browser.

## Building

The frontend can currently be built for the TrimUI Model S, FunKey S, and Linux (useful for testing and debugging).

First, fetch the repo with submodules:

```
git clone --recurse-submodules https://git.crowdedwood.com/picoarch
```

### Linux instructions

To build picoarch itself, you need libSDL 1.2, libpng, and libasound. Different cores may need additional dependencies.

After that, `make` builds picoarch and all supported cores into this directory.

### TrimUI instructions

To build for TrimUI, you n

To build generic binaries:

```
make platform=trimui
```

If you want to build for MinUI, you need to install [libmmenu](https://github.com/shauninman/libmmenu) into the toolchain. Then:

```
make platform=trimui MINUI=1
```

`MINUI=1` will change save/config/system paths to match MinUI standards. If you just want to include mmenu, you can run:

```
make platform=trimui MMENU=1
```

To build for distribution:

```
make platform=trimui dist-gmenu
make platform=trimui MINUI=1 dist-minui
```

These will output a directory structure that can be moved onto the SD card into `pkg/gmenunx` or `pkg/MinUI`.

Or run

```
make platform=trimui picoarch.zip
```

To build a .zip file ready for SD card.

### FunKey S instructions

To build for FunKey S, you need a toolchain first, following [instructions](https://doc.funkey-project.com/developer_guide/tutorials/build_system/build_program_using_sdk/) on the FunKey wiki.

To build generic binaries:

```
make platform=funkey-s
```

To build a specific core as .opk file:

```
make platform=funkey-s picoarch-gambatte.opk
```

Or run

```
make platform=funkey-s picoarch-funkey-s.zip
```

To build a .zip file containing all .opk files.


### Other build options

To debug:

```
make DEBUG=1
```

To build a specific supported core:

```
make gpsp_libretro.so
```

To clean a core so it will be built again:

```
make clean-gpsp
```

To completely clean the repo (will delete, pull, and patch all core repos from scratch)

```
make force-clean
```

To build profiles for profile-guided optimization:

```
make PROFILE=GENERATE
```

To apply the generated profiles:

```
make PROFILE=APPLY
```

PGO can give noticeable speed improvements with some emulators.

## Notes on cores

In order to make development and testing easier, the Makefile will pull and build supported cores.

You will have to make changes when adding a core, since TrimUI is not a supported libretro platform. picoarch has a `patches/` directory containing needed changes to make cores work well in picoarch. Patches are applied in order after checking out the repository. 

At a minimum, you need to add a `platform=trimui` section to the core Makefile if you are building for trimui.

Some features and fixes are also included in `patches` -- it would be best to try to upstream them.

picoarch keeps the running core name in a global variable. This is used to override defaults and core settings to work more nicely within picoarch. Overrides based on core name are kept in `overrides/` and referenced in `overrides.c`. These are used to:

- Shorten core option text and change defaults for small screen / low power devices
- Rename buttons to match the core's system
- Reference frameskip core options to make fast-forward faster
- Display extra options or hide unnecessary options
