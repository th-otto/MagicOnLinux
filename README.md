# MagicOnLinuX
<img alt="Logo" src="assets/Logo.png" width="80">

An Atari ST/TT emulator for Linux, SDL2 based, running MagiC OS only.

This is kind of successor of:

* *MagiC* for Atari,
* *MagicMac* for Classic Mac OS,
* *MagicMac X* for MacOS X on PPC (32-bit application) and
* *AtariX* for macOS up to 10.13 "High Sierra" (32-bit application).

Basically MagicOnLinux is AtariX with removed GUI and replaced host file system on the emulator side. In particular the Carbon based MacXFS was replaced with a Linux/Posix based host XFS. Additionally, due to the 64-bit host architecture, any callback from emulated to emulator had to be replaced with a new, different concept. There are various compromises, because the MagiC kernel file remained unchanged, and this one unfortunately contains a significant part of the old MacXFS, that is not suitable for Posix calls. A clean solution would have been to replace, or mainly remove, the 68k part of the XFS and run the host XFS completely in the host environment.

# How To Build (Linux, tested with Ubuntu 24.04)

- sudo apt install libsdl2-dev
- cd ~/Documents
- git clone https://gitlab.com/AndreasK/magiclinux
- git clone https://gitlab.com/AndreasK/AtariX
- mkdir magiclinux/build
- pushd magiclinux/build
- cmake -DCMAKE_BUILD_TYPE=Release ..
- make
- popd
- cp -rp AtariX/src/AtariX-MT/AtariX/rootfs-common Atari-rootfs
- cp -p magiclinux/kernel/HOSTBIOS/MAGICLIN.OS Atari-rootfs/
- rsync -a AtariX/src/AtariX-MT/AtariX/English.lproj/rootfs/ Atari-rootfs/

Replace "English" with "de" or "fr" for German or French.

The MAGICLIN.OS kernel is currently only available in German, but you may compile and link your own.

You might replace CMAKE_BUILD with "Debug" or omit this parameter.

Alternatively you can put your Atari root file system (drive C:) and kernel (MAGICLIN.OS) anywhere and configure the emulator accordingly.

# How To Run

Run the application with "magiclinux/build/magic-on-linux".

Use parameter "-h" or "--help" for an explanation of the parameters.

Source files for the Atari code (MagiC kernel and applications) are also available in their respective repository, see below.

# Screenshots
<img alt="No" src="assets/Atari-Desktop - BW.png" width="1024">
<img alt="Yes" src="assets/Settings.png" width="640">


# Supported

* Emulates MC68020 processor
* Arbitrary screen sizes and colour depths
* Zoom, helpful for original 640x400 or 640x200 resolution
* Full access to host file system, up to root

# Bugs and Agenda

* Include German, French and English localisation of emulator and emulated system without needing to clone AtariX repository.
* Ability to mount disk and floppy disk images.
* Musashi emulator sources should be synchronised with latest version (see below).
* Atari root file system (like MAGIC_C) folder should be automatically created and localised.
* Clean shutdown of emulated system.
* Add support for compilation on macOS.
* Tell me.

# License

The MagicLinux emulator is licensed according to GPLv3, see LICENSE file.

# External Licenses

**AtariX application for older macOS, multilingual root FS and sources**
see: https://gitlab.com/AndreasK/AtariX

**Atari Sources**
see: https://gitlab.com/AndreasK/Atari-Mac-MagiC-Sources

**Musashi 68k emulator in C**
Copyright 1998-2002 Karl Stenerud
Source: https://github.com/kstenerud/Musashi
License: https://github.com/kstenerud/Musashi/readme.txt

**SDL library:**
Source: https://www.libsdl.org
Copyright: paultaylor@jthink.net
License: http://www.gzip.org/zlib/zlib_license.html

**Atari VDI Drivers**
Copyright: Wilfried und Sven Behne, License: mit freundlicher Genehmigung
