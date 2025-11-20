# MagicOnLinuX
<img alt="Logo" src="assets/Logo.png" width="80">

An Atari ST/TT High Level Emulator for Linux, SDL2 based, running MagiC OS only.

This is kind of successor of:

* *KAOS 1.2* for Atari,
* *KAOS 1.4.2* for Atari,
* *MagiC* for Atari,
* *MagicMac* for Classic Mac OS (68k and PPC),
* *MagicMac X* for MacOS X on PPC (32-bit application) and
* *AtariX* for macOS up to 10.13 "High Sierra" (32-bit application).

Basically MagicOnLinux is AtariX with removed GUI and replaced host file system on the emulator side. In particular the Carbon based MacXFS was replaced with a Linux/Posix based host XFS. Additionally, due to the 64-bit host architecture, any callback from emulated to emulator had to be replaced with a new, different concept. There are various compromises, because the MagiC kernel file partially remained unchanged, and this one unfortunately contains a significant part of the old MacXFS, that is not suitable for Posix calls. A clean solution will be to replace, or mainly remove, the 68k part of the XFS and run the host XFS completely in the host environment.

# How To Build (Linux, tested with Ubuntu 24.04)

- sudo apt install libsdl2-dev gxmessage
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

Without gxmessage you will not see error message dialogues, instead the text will be printed to stderr only.

The MAGICLIN.OS kernel is currently only available in German, but you may compile and link your own.

You might replace CMAKE_BUILD with "Debug" or omit this parameter.

Alternatively you can put your Atari root file system (drive C:) and kernel (MAGICLIN.OS) anywhere and configure the emulator accordingly.

# How To Run

Run the application with "magiclinux/build/magic-on-linux".

Use parameter "-h" or "--help" for an explanation of the parameters.

Especially helpful: parameter "-e" to open the configuration file in a text editor.

Atari RAM size is given in bytes and excludes video memory, which is managed separately.

Source files for the Atari code (MagiC kernel and applications) are also available in their respective repository, see below.

# Screenshots
<img alt="No" src="assets/Atari-Desktop - BW.png" width="1024">
<img alt="Yes" src="assets/Settings.png" width="640">


# Supported

* Emulates MC68020 processor
* Arbitrary screen sizes and colour depths
* Zoom, helpful for original 640x400 or 640x200 resolution
* Full access to host file system, up to root
* Mounts Atari volume or floppy disk images
* Copy/paste clipboard text between host and emulated system.
* Command line option to convert Atari text files to UTF-8 and vice-versa, including line endings between CR/LF and LF.
* Mount file systems, folder or image, via Drag&Drop, readable or read-only.
* Some command line switches are provided to override config file settings.

# Remarks

* The MagiC task manager is activated via Ctrl-Alt-^ (the key below Esc) on German keyboards, because Ctrl-Alt-Esc is reserved in Linux.

# Bugs and Agenda

* Include German, French and English localisation of emulator and emulated system without needing to clone AtariX repository.
* Support MBR based partition tables, i.e. disk images, not only volume images.
* Musashi emulator sources should be synchronised with latest version (see below).
* Atari root file system (like MAGIC_C) folder should be automatically created and localised.
* Add support for compilation on macOS.

# Example Command to Create a Volume Image:

* "-S 512" for 512 bytes per sector
* "-s 2" for 2 sectors per cluster
* "-F 32" for FAT32, FAT16 and FAT12
* Specify "-v" for verbose output
* Name written to boot sector is "FAT32_1M"
* Filename is "vol-fat32-1M.img"
* Volume size is 1024 kB (1 MB)

mkfs.vfat -S 512 -s 2 -F 32 -v -n "FAT32_1M" -C vol-fat32-1M.img 1024

# Example Command to Create a 720k Floppy Disk Image

mkfs.msdos -C floppy720k.img 720

# License

The MagicOnLinux emulator is licensed according to GPLv3, see LICENSE file.

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
