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
 * Lesser General Public License for more details.
 *
 * Copyright (C) 2013-2019 SevenBits
 *
 */

#include <efi.h>
#include <efilib.h>
#include <stdbool.h>

#include "main.h"
#include "menu.h"
#include "utils.h"
#include "hardware.h"
#include "config.h"

const EFI_GUID enterprise_variable_guid = {0xd92996a6, 0x9f56, 0x48fc, {0xc4, 0x45, 0xb9, 0x0f, 0x23, 0x98, 0x6d, 0x4a}};
const EFI_GUID grub_variable_guid = {0x8BE4DF61, 0x93CA, 0x11d2, {0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B,0x8C}};

CHAR16 *banner = L"Welcome to Enterprise! - Version %d.%d.%d\n";

EFI_LOADED_IMAGE *this_image = NULL;
EFI_FILE *root_dir;

EFI_HANDLE global_image = NULL; // EFI_HANDLE is a typedef to a VOID pointer.
BootableLinuxDistro *distributionListRoot;

/* entry function for EFI */
EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab) {
	/* Setup key GNU-EFI library and its functions first. */
	EFI_STATUS err; // Define an error variable.
	
	InitializeLib(image_handle, systab); // Initialize EFI.
	console_text_mode(); // Put the console into text mode. If we don't do that, the image of the Apple
	                     // boot manager will remain on the screen and the user won't see any output
	                     // from the program.
	SetupDisplay();
	global_image = image_handle;
	
	err = uefi_call_wrapper(BS->HandleProtocol, 3, image_handle, &LoadedImageProtocol, (void *)&this_image);
	if (EFI_ERROR(err)) {
		Print(L"Error: could not find loaded image: %d\n", err);
		uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
		return err;
	}
	
	root_dir = LibOpenRoot(this_image->DeviceHandle);
	if (!root_dir) {
		DisplayErrorText(L"Unable to open root directory.\n");
		uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
		return EFI_LOAD_ERROR;
	}
	
	/* Setup global variables. */
	// Set all present options to be false (i.e off).
	SetMem(preset_options_array, PRESET_OPTIONS_SIZE * sizeof(BOOLEAN), 0);
	
	/* Print the welcome message. */
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK); // Set the text color.
	uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut); // Clear the screen.
	Print(banner, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH); // Print the welcome information.
	uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
	uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, FALSE); // Disable display of the cursor.
	
	BOOLEAN can_continue = TRUE;
	
	/* Check to make sure that we have our configuration file and GRUB bootloader. */
	if (!FileExists(root_dir, L"\\efi\\boot\\enterprise.cfg")) {
		can_continue = FALSE;
	} else {
		ReadConfigurationFile(L"\\efi\\boot\\enterprise.cfg");
	}
	
	// Verify if the configuration file is valid.
	if (!distributionListRoot) {
		DisplayErrorText(L"Error: configuration file parsing error.\n");
		can_continue = FALSE;
	}
	
	// Check for GRUB.
	if (!FileExists(root_dir, L"\\efi\\boot\\boot.efi")) {
		DisplayErrorText(L"Error: can't find GRUB bootloader!\n");
		can_continue = FALSE;
	}
	
	// Check if there is a persistence file present.
	// TODO: Support distributions other than Ubuntu.
	if (FileExists(root_dir, L"\\casper-rw") && can_continue) {
		DisplayColoredText(L"Found a persistence file! You can enable persistence by " \
							"selecting it in the Modify Boot Settings screen.\n");
		
		preset_options_array[4] = true;
	}
	
	// Display the menu where the user can select what they want to do.
	if (can_continue) {
		if (!shouldAutoboot) {
			DisplayMenu();
		} else {
			// Don't allow the user to overflow.
			if (autobootIndex > (UINTN)distroCount) {
				DisplayErrorText(L"Cannot continue because you have selected an invalid distribution.\nRestarting...\n");
				uefi_call_wrapper(BS->Stall, 1, 1000 * 1000);
				return EFI_LOAD_ERROR;
			}

			BootLinuxWithOptions(L"", autobootIndex);
			uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
		}
	} else {
		DisplayErrorText(L"Cannot continue because core files are missing or damaged.\nRestarting...\n");
		uefi_call_wrapper(BS->Stall, 1, 1000 * 1000);
		return EFI_LOAD_ERROR;
	}
	
	return EFI_SUCCESS;
}

EFI_STATUS BootLinuxWithOptions(CHAR16 *params, UINT16 distribution) {
	EFI_STATUS err;
	EFI_HANDLE image;
	EFI_DEVICE_PATH *path = NULL;
	
	uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
	
	// We need to move forward to the proper distribution struct.
	BootableLinuxDistro *conductor = distributionListRoot->next;
	
	INTN i; for (i = 0; i < distribution && conductor != NULL; i++, conductor = conductor->next);
	LinuxBootOption *boot_params = conductor->bootOption;
	if (!boot_params) {
		DisplayErrorText(L"Error: couldn't get Linux distribution boot settings.\n");
		return EFI_LOAD_ERROR;
	}
	
	CHAR8 *kernel_path = boot_params->kernel_path;
	CHAR8 *initrd_path = boot_params->initrd_path;
	CHAR8 *boot_folder = boot_params->boot_folder;
	CHAR8 *iso_path = boot_params->iso_path;

	// Convert the kernel options string from a UTF16 string into an ASCII C string.
	// We need to do this because GNU-EFI uses Unicode internally but we can only pass ASCII
	// C strings to GRUB.
	//
	// We also concatenate the kernel options given as part of the Enterprise configuration
	// file with the user selected kernel options from the Advanced menu. The user selected
	// options should override those given in the configuration file.
	CHAR8 *sized_str = UTF16toASCII(params, StrLen(params) + 1);
	CHAR8 *kernel_parameters = NULL;
	kernel_parameters = AllocatePool(sizeof(CHAR8) * (strlena(sized_str) + strlena(boot_params->kernel_options)));
	if (boot_params->kernel_options && strlena(boot_params->kernel_options) > 0) {
		strcpya(kernel_parameters, boot_params->kernel_options);
		strcata(kernel_parameters, sized_str);
	} else {
		strcpya(kernel_parameters, sized_str);
	}
	
	efi_set_variable(&grub_variable_guid, L"Enterprise_LinuxBootOptions", kernel_parameters,
		sizeof(kernel_parameters[0]) * (strlena(kernel_parameters) + 1), FALSE);
	efi_set_variable(&grub_variable_guid, L"Enterprise_LinuxKernelPath", kernel_path,
		sizeof(kernel_path[0]) * (strlena(kernel_path) + 1), FALSE);
	efi_set_variable(&grub_variable_guid, L"Enterprise_InitRDPath", initrd_path,
		sizeof(initrd_path[0]) * (strlena(initrd_path) + 1), FALSE);
	efi_set_variable(&grub_variable_guid, L"Enterprise_ISOPath", iso_path,
		sizeof(iso_path[0]) * (strlena(iso_path) + 1), FALSE);
	efi_set_variable(&grub_variable_guid, L"Enterprise_BootFolder", boot_folder,
		sizeof(boot_folder[0]) * (strlena(boot_folder) + 1), FALSE);
	
	// Load the EFI boot loader image into memory.
	path = FileDevicePath(this_image->DeviceHandle, L"\\efi\\boot\\boot.efi");
	err = uefi_call_wrapper(BS->LoadImage, 6, TRUE, global_image, path, NULL, 0, &image);
	if (EFI_ERROR(err)) {
		DisplayErrorText(L"Error loading image: ");
		Print(L"%r\n", err);
		uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
		FreePool(path);
		
		return EFI_LOAD_ERROR;
	}
	
	// Start the EFI boot loader.
	uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut); // Clear the screen.
	err = uefi_call_wrapper(BS->StartImage, 3, image, NULL, NULL);
	if (EFI_ERROR(err)) {
		DisplayErrorText(L"Error starting image: ");
		Print(L"%r\n", err);
		uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
		FreePool(path);
		
		return EFI_LOAD_ERROR;
	}

	// Should never return.
	return EFI_SUCCESS;
}
