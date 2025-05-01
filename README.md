# Picoarch for ClockworkPi Picocalc with Luckfox Lyra

This is a quick and rought port of Picoarch, a libretro frontend designed for small screens and low power, for the ClockworkPi Picocalc with Luckfox Lyra.

Picoarch uses libpicofe and SDL to create a small frontend to libretro cores. This minor patch will provide 320x320 display support and the necessary cross compilaton mods.

All the kudos go to the original author, neonloop, that had done a really good job, and to Hisptoot that had prepared the required picocalc/lyra dev environment and wrote the picocalc drivers.

The only modification i made was adapt Makefile and adapt GFX scaling option to 320x320 pixel.

## Linux on Picocalc/Luckfox
Hisptoot has created a buildroot environment to write and compile the required kernel driver for picocalc lcd, keyboard and sound.

:warning: audio require and easy hardware modification, both to the Lucfox Lyra board and Picocalc main board. This require a solder and minimal experience, don´t blame me for fire, explosion or anything bad should happen.

All info about that, precompiled images, and update are available :

[Picocalc ¨Luckfox lyra on picocalc¨ forum thread](https://forum.clockworkpi.com/t/luckfox-lyra-on-picocalc/16280)

[Hipstoot google drive - prebuild tools SD card images and updates](https://drive.google.com/drive/folders/1TBEso7NFkO7e6z8iEBywjxi4EtJHSz4F)

[Hipstoot sourcecode repository](https://github.com/hisptoot/picocalc_luckfox_lyra)

## Picocalc/Luckfox Lyra dev environment
:warning: Compilation was done on Kde Neon 6.3, but any modern Linux distro may be good.

To compile this you need Hisptoot buildroot tools and library.
You can find in the **sdk-buildroot** folder in Hisptoot [google drive](https://drive.google.com/drive/folders/1TBEso7NFkO7e6z8iEBywjxi4EtJHSz4F)

Download and unzip in your choosen folder, let's say **~/luckfox**, get into directory and execute :

```
cd ~/luckfox/arm-buildroot-linux-gnueabihf_sdk-buildroot
./relocate-sdk.sh
```

To set cross compile envionment, before compiling code meant to run on device, you should execute :

```
source ~/luckfox/environment-setup
```

that will set path to use device specific tool, such as compiler, linker et al.

## Fetch code and compile 

First, fetch the repo with submodules:

```
cd ~/luckfox/
git clone --recurse-submodules https://github.com/gurubook/picoarch.git
```

To build picoarch itself, you need libSDL 1.2, libpng, and libasound. Different cores may need additional dependencies, add with the package manager of choice.

After that, `make device=picolyra` builds picoarch and all supported cores into this directory.

```
cd cd ~/luckfox/picoarch
make device=picolyra
```

## Install on picocalc
To perform transfer to picocal running linux you can use `adb` command connecting your computer to the Lyra USB-C port.

## Running picoarch
Running picoarch on picocalc require a sound device, so you need to load `insmod /usr/lib/picocalc_snd_pwm.ko`
To perform this at boot, you can remove comment to the right line in `/etc/init.d/S09picocalc`

## Neonloop original code
[Original Author README](README_ORIG.md)
[Original license](LICENSE)
[Original Picoarch GIT repo](https://git.crowdedwood.com/picoarch/)
