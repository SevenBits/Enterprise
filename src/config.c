/*
 * Tool intended to help facilitate the process of booting Linux on Intel
 * Macintosh computers made by Apple from a USB stick or similar.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * Copyright (C) 2017 SevenBits
 *
 */

#include <efi.h>
#include <efilib.h>
#include "config.h"
#include "distribution.h"
#include "utils.h"

BOOLEAN shouldAutoboot;
UINTN autobootIndex = 0;
INTN distroCount = -1; // start at -1 due to an error on my part.

void ReadConfigurationFile(const CHAR16 * const name) {
	/* This will always stay consistent, otherwise we'll lose the list in memory.*/
	distributionListRoot = AllocateZeroPool(sizeof(BootableLinuxDistro));
	if (!distributionListRoot) {
		DisplayErrorText(L"Unable to allocate memory for linked list.\n");
	}

	BootableLinuxDistro *conductor; // Will point to each node as we traverse the list.
	conductor = distributionListRoot; // Start by pointing at the first element.
	
	CHAR8 *contents;
	UINTN read_bytes = FileRead(root_dir, name, &contents);
	if (read_bytes == 0) {
		DisplayErrorText(L"Error: Couldn't read configuration information.\n");
		return;
	}
	
	UINTN position = 0;
	CHAR8 *key, *value, *distribution, *boot_folder;
	while ((GetConfigurationKeyAndValue(contents, &position, &key, &value))) {
		/* 
		 * We require the user to specify an entry, followed by the file name and
		 * any information required to boot the Linux distribution.
		 */
		// The autoboot entry was enabled.
		if (strcmpa((CHAR8 *)"autoboot", key) == 0) {
			shouldAutoboot = TRUE;

			// Check if they've given us a parameter; if they have, check if it's a valid
			// integer and then parse it.
			// The user can currently only autoboot the first ten entries.
			if (strlena(value) == 1 && (*value >= 48 && *value <= 57)) {
				autobootIndex = *value - '0';
			}
		}
		// The user has put a given a distribution entry.
		else if (strcmpa((CHAR8 *)"entry", key) == 0) {
			BootableLinuxDistro *new = AllocateZeroPool(sizeof(BootableLinuxDistro));
			if (!new) {
				DisplayErrorText(L"Failed to allocate memory for distribution entry.");
			}

			new->bootOption = AllocateZeroPool(sizeof(LinuxBootOption));
			AllocateMemoryAndCopyChar8String(new->bootOption->name, value);
			AllocateMemoryAndCopyChar8String(new->bootOption->iso_path, (CHAR8 *)"boot.iso"); // Set a default value.
			
			conductor->next = new;
			new->next = NULL;
			conductor = conductor->next; // subsequent operations affect the new link in the chain
			distroCount++;
		}
		// The user has given us a distribution family.
		else if (strcmpa((CHAR8 *)"family", key) == 0) {
			AllocateMemoryAndCopyChar8String(distribution, value);
			AllocateMemoryAndCopyChar8String(conductor->bootOption->distro_family, value);
			AllocateMemoryAndCopyChar8String(conductor->bootOption->kernel_path, KernelLocationForDistributionName(distribution, &boot_folder));
			AllocateMemoryAndCopyChar8String(conductor->bootOption->initrd_path, InitRDLocationForDistributionName(distribution));
			AllocateMemoryAndCopyChar8String(conductor->bootOption->boot_folder, boot_folder);
			//Print(L"Boot folder: %s\n", ASCIItoUTF16(boot_folder, strlena(boot_folder)));
			// If either of the paths are a blank string, then you've got an
			// unsupported distribution or a typo of the distribution name.
			if (strcmpa((CHAR8 *)"", conductor->bootOption->kernel_path) == 0 ||
				strcmpa((CHAR8 *)"", conductor->bootOption->initrd_path) == 0) {
				Print(L"Distribution family %a is not supported.\n", value);
				
				FreePool(conductor->bootOption);
				distributionListRoot = NULL;
				return;
			}
		// The user is manually specifying information; override any previous values.
		} else if (strcmpa((CHAR8 *)"kernel", key) == 0) {
			if (strposa(value, ' ') != -1) {
				/*
				 * There's a space after the kernel name; the user has given us additional kernel parameters.
				 * Separate the kernel path and options and copy them into their respective positions in the
				 * boot options struct.
				 */
				// Initialize variables and free memory that we might be overwriting soon.
				INTN spaceCharPos = strposa(value, ' ');
				INTN kernelStringLength = sizeof(CHAR8) * spaceCharPos;
				if (conductor->bootOption->kernel_path) FreePool(conductor->bootOption->kernel_path);
				conductor->bootOption->kernel_path = NULL;

				// Allocate memory and begin the copy.
				conductor->bootOption->kernel_path = AllocatePool(kernelStringLength + 1); // +1 is for null terminator
				if (!conductor->bootOption->kernel_path) {
					DisplayErrorText(L"Unable to allocate memory.");
					Print(L" %s %d\n", __FILE__, __LINE__);
				}

				strncpya(conductor->bootOption->kernel_path, value, spaceCharPos);
				*(conductor->bootOption->kernel_path + kernelStringLength) = '\0';
				//Print(L"conductor->bootOption->kernel_path = %a\n", conductor->bootOption->kernel_path);

				// Begin dealing with the kernel parameters and copy them too.
				CHAR8 *params = value + spaceCharPos + 1; // Start the copy just past the space character
				AllocateMemoryAndCopyChar8String(conductor->bootOption->kernel_options, params);
				//Print(L"conductor->bootOption->kernel_options = %a\n", conductor->bootOption->kernel_options);
			} else {
				AllocateMemoryAndCopyChar8String(conductor->bootOption->kernel_path, value);
			}
		} else if (strcmpa((CHAR8 *)"initrd", key) == 0) {
			AllocateMemoryAndCopyChar8String(conductor->bootOption->initrd_path, value);
		} else if (strcmpa((CHAR8 *)"iso", key) == 0) {
			strcpya(conductor->bootOption->iso_path, value);
			
			CHAR16 *temp = ASCIItoUTF16(value, strlena(value));
			if (!FileExists(root_dir, temp)) {
				Print(L"Warning: ISO file %a not found.\n", value);
			}
			FreePool(temp);
		} else if (strcmpa((CHAR8 *)"root", key) == 0) {
			AllocateMemoryAndCopyChar8String(conductor->bootOption->boot_folder, value);
		} else {
			Print(L"Unrecognized configuration option: %a.\n", key);
		}
	}
	
	FreePool(contents);
	//Print(L"Done reading configuration file.\n");
}
