/*
 * Tool intended to help facilitate the process of booting Linux on Intel
 * Macintosh computers made by Apple from a USB stick or similar.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * Copyright (C) 2013 SevenBits
 *
 */

#pragma once
#ifndef _main_h
#define _main_h

#define EFI_2_20_SYSTEM_TABLE_REVISION ((2<<16) | (20))
#define EFI_2_10_SYSTEM_TABLE_REVISION ((2<<16) | (10))
#define EFI_2_00_SYSTEM_TABLE_REVISION ((2<<16) | (00))
#define EFI_1_10_SYSTEM_TABLE_REVISION ((1<<16) | (10))
#define EFI_1_02_SYSTEM_TABLE_REVISION ((1<<16) | (02))

#define PRESET_OPTIONS_SIZE 20

extern CHAR16 *banner;
#define VERSION_MAJOR 0
#define VERSION_MINOR 3
#define VERSION_PATCH 1

#define AllocateMemoryAndCopyChar8String(dest, src) \
	if (dest) FreePool(dest); dest = NULL; \
	dest = AllocatePool(sizeof(CHAR8) * (strlena(src) + 1)); \
	strcpya(dest, src); \

typedef struct LinuxBootOption {
	CHAR8 *name;
	CHAR8 *file_name;
	CHAR8 *distro_family;
	CHAR8 *kernel_path;
	CHAR8 *kernel_options;
	CHAR8 *initrd_path;
	CHAR8 *boot_folder;
	CHAR8 *iso_path;
} LinuxBootOption;

typedef struct BootableLinuxDistro {
	LinuxBootOption *bootOption;
	struct BootableLinuxDistro *next;
} BootableLinuxDistro;

EFI_STATUS BootLinuxWithOptions(CHAR16 *, UINT16);

extern const EFI_GUID enterprise_variable_guid;
extern const EFI_GUID grub_variable_guid;

extern UINTN numberOfDisplayRows, numberOfDisplayColoumns, highestModeNumberAvailable;
extern BOOLEAN preset_options_array[PRESET_OPTIONS_SIZE];

extern BootableLinuxDistro *distributionListRoot;

#endif
