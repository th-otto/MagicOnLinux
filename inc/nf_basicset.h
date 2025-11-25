/*
 * This file was taken from the ARAnyM project, and adapted to MagicOnLinux:
 *
 * nf_basicset.h - NatFeat Basic Set - declaration
 *
 * Copyright (c) 2002-2004 Petr Stehlik of ARAnyM dev team (see AUTHORS)
 *
 * This file is part of the ARAnyM project which builds a new and powerful
 * TOS/FreeMiNT compatible virtual machine running on almost any hardware.
 *
 * ARAnyM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ARAnyM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _NF_BASICSET_H
#define _NF_BASICSET_H

#include "nf_base.h"

extern NF_Base const nf_name;
extern NF_Base const nf_version;
extern NF_Base const nf_shutdown;
extern NF_Base const nf_stderr;
extern NF_Base const nf_exit;

#if defined(__APPLE__)
#define OS_TYPE "macOS"
#define HOST_SCREEN_DRIVER "Quartz"
#elif defined(__ANDROID__)
#define OS_TYPE "Android"
#define HOST_SCREEN_DRIVER "X"
#elif defined(__linux__)
#define OS_TYPE "Linux"
#define HOST_SCREEN_DRIVER "X"
#elif defined(_WIN32) || defined(__CYGWIN__)
#define OS_TYPE "Windows"
#define HOST_SCREEN_DRIVER "GDI"
#else
#define OS_TYPE "Unknown"
#define HOST_SCREEN_DRIVER "Unknown"
#endif

#ifdef __x86_64__
#define HOST_CPU_TYPE "x86-64"
#elif defined(__i386__)
#define HOST_CPU_TYPE "x86"
#elif defined(__aarch64__)
#define HOST_CPU_TYPE "aarch64"
#elif defined(__arm__)
#define HOST_CPU_TYPE "arm"
#else
#define HOST_CPU_TYPE "Unknown"
#endif

#define NAME_STRING "MagicOnLinux"

#endif /* _NF_BASICSET_H */
