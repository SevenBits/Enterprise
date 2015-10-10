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
#include <stdbool.h>

#include "main.h"
#include "menu.h"
#include "utils.h"
#include "distribution.h"

const EFI_GUID enterprise_variable_guid = {0xd92996a6, 0x9f56, 0x48fc, {0xc4, 0x45, 0xb9, 0x0f, 0x23, 0x98, 0x6d, 0x4a}};
const EFI_GUID grub_variable_guid = {0x8BE4DF61, 0x93CA, 0x11d2, {0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B,0x8C}};

static void ReadConfigurationFile(const CHAR16 *);

static EFI_STATUS console_text_mode(VOID);
static EFI_STATUS SetupDisplay(VOID);
UINTN numberOfDisplayRows, numberOfDisplayColoumns, highestModeNumberAvailable = 0;
CHAR16 *banner = L"Welcome to Enterprise! - Version %d.%d.%d\n";

EFI_LOADED_IMAGE *this_image = NULL;
static EFI_FILE *root_dir;

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
		// Check if we have an old-style configuration file instead.
		
		if (!FileExists(root_dir, L"\\efi\\boot\\.MLUL-Live-USB")) {
			DisplayErrorText(L"Error: can't find configuration file.\n");
			can_continue = FALSE;
		} else {
			DisplayErrorText(L"Warning: old-style configuration file found, please upgrade to the new format\n");
			ReadConfigurationFile(L"\\efi\\boot\\.MLUL-Live-USB");
		}
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
		DisplayErrorText(L"Error: can't find GRUB bootloader!.\n");
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
		DisplayMenu();
	} else {
		DisplayErrorText(L"Cannot continue because core files are missing or damaged.\nRestarting...\n");
		uefi_call_wrapper(BS->Stall, 1, 1000 * 1000);
		return EFI_LOAD_ERROR;
	}
	
	return EFI_SUCCESS;
}

static EFI_STATUS SetupDisplay(VOID) {
	// Set the display to use the highest available resolution.
	EFI_STATUS err = EFI_SUCCESS;
	
	while (!EFI_ERROR(err)) {
		err = uefi_call_wrapper(ST->ConOut->QueryMode, 4, ST->ConOut, highestModeNumberAvailable, &numberOfDisplayRows, &numberOfDisplayColoumns);
		Print(L"Detected mode %d: %d x %d.\n", highestModeNumberAvailable, numberOfDisplayRows, numberOfDisplayColoumns);
		
		if (!EFI_ERROR(err)) highestModeNumberAvailable++;
	}
	
	Print(L"Setting display to be in mode %d.\n", highestModeNumberAvailable - 1);
	err = uefi_call_wrapper(ST->ConOut->SetMode, 2, ST->ConOut, highestModeNumberAvailable - 1);
	if (EFI_ERROR(err)) {
		DisplayErrorText(L"Can't set display mode! ");
		Print(L"%r\n", err);
		uefi_call_wrapper(BS->Stall, 1, 500 * 1000);
	}
	
	return err;
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
	
	CHAR8 *sized_str = UTF16toASCII(params, StrLen(params) + 1);
	CHAR8 *kernel_parameters = NULL;
	kernel_parameters = AllocatePool(sizeof(CHAR8) * (strlena(sized_str) + strlena(boot_params->kernel_options)));
	strcpya(kernel_parameters, sized_str);
	if (boot_params->kernel_options && strlena(boot_params->kernel_options) > 0) {
		strcata(kernel_parameters, boot_params->kernel_options);
	}
	
	efi_set_variable(&grub_variable_guid, L"Enterprise_LinuxBootOptions", kernel_parameters,
		sizeof(kernel_parameters[0]) * strlena(kernel_parameters) + 1, FALSE);
	efi_set_variable(&grub_variable_guid, L"Enterprise_LinuxKernelPath", kernel_path,
		sizeof(kernel_path[0]) * strlena(kernel_path) + 1, FALSE);
	efi_set_variable(&grub_variable_guid, L"Enterprise_InitRDPath", initrd_path,
		sizeof(initrd_path[0]) * strlena(initrd_path) + 1, FALSE);
	efi_set_variable(&grub_variable_guid, L"Enterprise_ISOPath", iso_path,
		sizeof(iso_path[0]) * strlena(iso_path) + 1, FALSE);
	efi_set_variable(&grub_variable_guid, L"Enterprise_BootFolder", boot_folder,
		sizeof(boot_folder[0]) * strlena(boot_folder) + 1, FALSE);
	
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
	
	uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
	// Should never return.
	return EFI_SUCCESS;
}

static void ReadConfigurationFile(const CHAR16 *name) {
	/* This will always stay consistent, otherwise we'll lose the list in memory.*/
	distributionListRoot = AllocateZeroPool(sizeof(BootableLinuxDistro));
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
		// The user has put a given a distribution entry.
		if (strcmpa((CHAR8 *)"entry", key) == 0) {
			BootableLinuxDistro *new = AllocateZeroPool(sizeof(BootableLinuxDistro));
			new->bootOption = AllocateZeroPool(sizeof(LinuxBootOption));
			AllocateMemoryAndCopyChar8String(new->bootOption->name, value);
			AllocateMemoryAndCopyChar8String(new->bootOption->iso_path, (CHAR8 *)"boot.iso"); // Set a default value.
			
			conductor->next = new;
			new->next = NULL;
			conductor = conductor->next; // subsequent operations affect the new link in the chain
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

static EFI_STATUS console_text_mode(VOID) {
	#define EFI_CONSOLE_CONTROL_PROTOCOL_GUID \
		{ 0xf42f7782, 0x12e, 0x4c12, { 0x99, 0x56, 0x49, 0xf9, 0x43, 0x4, 0xf7, 0x21 } };

	struct _EFI_CONSOLE_CONTROL_PROTOCOL;

	typedef enum {
		EfiConsoleControlScreenText,
		EfiConsoleControlScreenGraphics,
		EfiConsoleControlScreenMaxValue,
	} EFI_CONSOLE_CONTROL_SCREEN_MODE;

	typedef EFI_STATUS (EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_GET_MODE)(
		struct _EFI_CONSOLE_CONTROL_PROTOCOL *This,
		EFI_CONSOLE_CONTROL_SCREEN_MODE *Mode,
		BOOLEAN *UgaExists,
		BOOLEAN *StdInLocked
	);

	typedef EFI_STATUS (EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_SET_MODE)(
		struct _EFI_CONSOLE_CONTROL_PROTOCOL *This,
		EFI_CONSOLE_CONTROL_SCREEN_MODE Mode
	);

	typedef EFI_STATUS (EFIAPI *EFI_CONSOLE_CONTROL_PROTOCOL_LOCK_STD_IN)(
		struct _EFI_CONSOLE_CONTROL_PROTOCOL *This,
		CHAR16 *Password
	);

	typedef struct _EFI_CONSOLE_CONTROL_PROTOCOL {
		EFI_CONSOLE_CONTROL_PROTOCOL_GET_MODE GetMode;
		EFI_CONSOLE_CONTROL_PROTOCOL_SET_MODE SetMode;
		EFI_CONSOLE_CONTROL_PROTOCOL_LOCK_STD_IN LockStdIn;
	} EFI_CONSOLE_CONTROL_PROTOCOL;

	EFI_GUID ConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;
	EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;
	EFI_STATUS err;

	err = LibLocateProtocol(&ConsoleControlProtocolGuid, (VOID **)&ConsoleControl);
	if (EFI_ERROR(err))
		return err;
	return uefi_call_wrapper(ConsoleControl->SetMode, 2, ConsoleControl, EfiConsoleControlScreenText);
}
