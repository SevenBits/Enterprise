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

#include <efi.h>
#include <efilib.h>

#include "distribution.h"

CHAR8* KernelLocationForDistributionName(CHAR8 *name, OUT CHAR8 **boot_folder) {
	if (strcmpa((CHAR8 *)"Debian", name) == 0) {
		*boot_folder = (CHAR8 *)"live";
		return (CHAR8 *)"/live/vmlinuz";
	} else if (strcmpa((CHAR8 *)"Ubuntu", name) == 0) {
		*boot_folder = (CHAR8 *)"casper";
		return (CHAR8 *)"/casper/vmlinuz.efi";
	} else {
		return (CHAR8 *)"";
	}
}

CHAR8* InitRDLocationForDistributionName(CHAR8 *name) {
	if (strcmpa((CHAR8 *)"Debian", name) == 0) {
		return (CHAR8 *)"/live/initrd.img";
	} else if (strcmpa((CHAR8 *)"Ubuntu", name) == 0) {
		return (CHAR8 *)"/casper/initrd.lz";
	} else {
		return (CHAR8 *)"";
	}
}
