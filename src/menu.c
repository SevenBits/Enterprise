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

#include "menu.h"
#include "main.h"
#include "utils.h"
#include "distribution.h"

static void ShowAboutPage(VOID);
static CHAR16 *boot_options;
static UINT8 distribution_id = -1;

#define KEYPRESS(keys, scan, uni) ((((UINT64)keys) << 32) | ((scan) << 16) | (uni))
#define EFI_SHIFT_STATE_VALID           0x80000000
#define EFI_RIGHT_CONTROL_PRESSED       0x00000004
#define EFI_LEFT_CONTROL_PRESSED        0x00000008
#define EFI_RIGHT_ALT_PRESSED           0x00000010
#define EFI_LEFT_ALT_PRESSED            0x00000020
#define EFI_CONTROL_PRESSED             (EFI_RIGHT_CONTROL_PRESSED|EFI_LEFT_CONTROL_PRESSED)
#define EFI_ALT_PRESSED                 (EFI_RIGHT_ALT_PRESSED|EFI_LEFT_ALT_PRESSED)
#define KEYPRESS(keys, scan, uni) ((((UINT64)keys) << 32) | ((scan) << 16) | (uni))
#define KEYCHAR(k) ((k) & 0xffff)
#define CHAR_CTRL(c) ((c) - 'a' + 1)

EFI_STATUS key_read(UINT64 *key, BOOLEAN wait) {
	#define EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID \
		{ 0xdd9e7534, 0x7762, 0x4698, { 0x8c, 0x14, 0xf5, 0x85, 0x17, 0xa6, 0x25, 0xaa } }

	struct _EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL;

	typedef EFI_STATUS (EFIAPI *EFI_INPUT_RESET_EX)(
		struct _EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This;
		BOOLEAN ExtendedVerification;
	);

	typedef UINT8 EFI_KEY_TOGGLE_STATE;

	typedef struct {
		UINT32 KeyShiftState;
		EFI_KEY_TOGGLE_STATE KeyToggleState;
	} EFI_KEY_STATE;

	typedef struct {
		EFI_INPUT_KEY Key;
		EFI_KEY_STATE KeyState;
	} EFI_KEY_DATA;

	typedef EFI_STATUS (EFIAPI *EFI_INPUT_READ_KEY_EX)(
		struct _EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This;
		EFI_KEY_DATA *KeyData;
	);

	typedef EFI_STATUS (EFIAPI *EFI_SET_STATE)(
				struct _EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This;
				EFI_KEY_TOGGLE_STATE *KeyToggleState;
	);

	typedef EFI_STATUS (EFIAPI *EFI_KEY_NOTIFY_FUNCTION)(
		EFI_KEY_DATA *KeyData;
	);

	typedef EFI_STATUS (EFIAPI *EFI_REGISTER_KEYSTROKE_NOTIFY)(
		struct _EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This;
		EFI_KEY_DATA KeyData;
		EFI_KEY_NOTIFY_FUNCTION KeyNotificationFunction;
		VOID **NotifyHandle;
	);

	typedef EFI_STATUS (EFIAPI *EFI_UNREGISTER_KEYSTROKE_NOTIFY)(
		struct _EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This;
		VOID *NotificationHandle;
	);

	typedef struct _EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL {
		EFI_INPUT_RESET_EX Reset;
		EFI_INPUT_READ_KEY_EX ReadKeyStrokeEx;
		EFI_EVENT WaitForKeyEx;
		EFI_SET_STATE SetState;
		EFI_REGISTER_KEYSTROKE_NOTIFY RegisterKeyNotify;
		EFI_UNREGISTER_KEYSTROKE_NOTIFY UnregisterKeyNotify;
	} EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL;

	EFI_GUID EfiSimpleTextInputExProtocolGuid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
	static EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *TextInputEx;
	static BOOLEAN checked;
	UINTN index;
	EFI_INPUT_KEY k;
	EFI_STATUS err;

	if (!checked) {
		err = LibLocateProtocol(&EfiSimpleTextInputExProtocolGuid, (VOID **)&TextInputEx);
		if (EFI_ERROR(err)) {
			TextInputEx = NULL;
		}

		checked = TRUE;
	}

	/* wait until key is pressed */
	if (wait) {
		if (TextInputEx) {
			uefi_call_wrapper(BS->WaitForEvent, 3, 1, &TextInputEx->WaitForKeyEx, &index);
		} else {
			uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &index);
		}
	}

	if (TextInputEx) {
		EFI_KEY_DATA keydata;
		UINT64 keypress;

		err = uefi_call_wrapper(TextInputEx->ReadKeyStrokeEx, 2, TextInputEx, &keydata);
		if (!EFI_ERROR(err)) {
			UINT32 shift = 0;

			/* do not distinguish between left and right keys */
			if (keydata.KeyState.KeyShiftState & EFI_SHIFT_STATE_VALID) {
				if (keydata.KeyState.KeyShiftState & (EFI_RIGHT_CONTROL_PRESSED|EFI_LEFT_CONTROL_PRESSED)) {
					shift |= EFI_CONTROL_PRESSED;
				}
				if (keydata.KeyState.KeyShiftState & (EFI_RIGHT_ALT_PRESSED|EFI_LEFT_ALT_PRESSED)) {
					shift |= EFI_ALT_PRESSED;
				}
			};

			/* 32 bit modifier keys + 16 bit scan code + 16 bit unicode */
			keypress = KEYPRESS(shift, keydata.Key.ScanCode, keydata.Key.UnicodeChar);
			if (keypress > 0) {
				*key = keypress;
				return EFI_SUCCESS;
			}
		}
	}

	/* Fallback for firmware which does not support SimpleTextInputExProtocol.
	 *
	 * This is also called in case ReadKeyStrokeEx did not return a key, because
	 * some broken firmwares offer SimpleTextInputExProtocol, but never acually
	 * handle any key. */
	err  = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &k);
	if (EFI_ERROR(err)) {
		return err;
	}

	*key = KEYPRESS(0, k.ScanCode, k.UnicodeChar);
	return EFI_SUCCESS;
}

EFI_STATUS DisplayDistributionSelector(struct BootableLinuxDistro *root, CHAR16 *bootOptions, BOOLEAN showBootOptions) {
	EFI_STATUS err = EFI_SUCCESS;
	
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK); // Set the text color.
	uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut); // Clear the screen.
	Print(banner, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH); // Print the welcome information.
	DisplayColoredText(L"\n    Boot Selector:\n");
	Print(L"    The following distributions have been detected on this USB.\n");
	Print(L"    Press the key corresponding to the number of the option that you want.\n\n");
	uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
	uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, FALSE); // Disable display of the cursor.
	
	// Print out the available Linux distributions on this USB.
	BootableLinuxDistro *conductor = root->next; // The first item is blank. I'll fix this later.
	INTN iteratorIndex = 0;
	while (conductor != NULL) {
		if (conductor->bootOption->name) {
			Print(L"    %d) %a\n", (iteratorIndex + 1), conductor->bootOption->name);
		}
		
		conductor = conductor->next;
		iteratorIndex++;
	}
	Print(L"\n    Press any other key to reboot the system.\n");
	
	// Get the key press.
	UINT64 key;
	err = key_read(&key, TRUE);
	INTN index = key - '0';
	index--; // C arrays start at index 0, but we start counting at 1, so compensate.
	
	if (index > iteratorIndex) {
		// Reboot the system.
		err = uefi_call_wrapper(RT->ResetSystem, 4, EfiResetCold, EFI_SUCCESS, 0, NULL);
		
		// Should never get here unless there's an error.
		Print(L"Error calling ResetSystem: %r", err);
		uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
	}
	
	if (showBootOptions) {
		// Save the selected distribution index for later.
		distribution_id = index;
		err = ConfigureKernel(bootOptions, preset_options_array, PRESET_OPTIONS_SIZE);
	} else {
		err = BootLinuxWithOptions(bootOptions, index);
	}
	
	return err; // Shouldn't get here.
}

EFI_STATUS DisplayMenu(void) {
	EFI_STATUS err;
	UINT64 key;
	boot_options = AllocateZeroPool(sizeof(CHAR16) * 150);
	
	start:
	
	/*
	 * Give the user some information as to what they can do at this point.
	 */
	DisplayColoredText(L"\n\n    Available boot options:\n");
	Print(L"    Press the key corresponding to the number of the option that you want.\n");
	Print(L"\n    1) Boot Linux from ISO file\n");
	Print(L"    2) Modify Linux kernel boot options (advanced!)\n");
	Print(L"\n    Press any other key to reboot the system.\n");
	
	err = key_read(&key, TRUE);
	if (key == '1') {
		DisplayDistributionSelector(distributionListRoot, L"", FALSE);
	} else if (key == '2') {
		DisplayDistributionSelector(distributionListRoot, L"", TRUE);
	} else if (key == 1507328) { // Escape key
		ShowAboutPage();
		uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
		Print(banner, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
		goto start;
	} else if (key == 720896) { // F1 key
		// Reset to use the default screen resolution. This is provided as a
		// counter-annoyance measure for screens which are incredibly large.
		uefi_call_wrapper(ST->ConOut->SetMode, 2, ST->ConOut, 0);
		numberOfDisplayRows = 80;
		numberOfDisplayColoumns = 25;
		uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, FALSE);
		goto start;
	} else {
		// Reboot the system.
		err = uefi_call_wrapper(RT->ResetSystem, 4, EfiResetCold, EFI_SUCCESS, 0, NULL);
		
		// Should never get here unless there's an error.
		Print(L"Error calling ResetSystem: %r", err);
		uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
	}
	
	return EFI_SUCCESS;
}

static void ShowAboutPage(VOID) {
	UINT64 sig = ST->Hdr.Signature;
	uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut); // Clear the screen.
	uefi_call_wrapper(ST->ConOut->SetCursorPosition, 2, 0, 0);
	
	// Print the Enterprise info and the system firmware version.
	DisplayColoredText(L"\n\n    Enterprise ");
	Print(L"%d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
	Print(L"    EFI: %s %d\n", ST->FirmwareVendor, ST->FirmwareRevision);
	if ((sig & EFI_2_20_SYSTEM_TABLE_REVISION) || (sig & EFI_2_10_SYSTEM_TABLE_REVISION) || (sig & EFI_2_00_SYSTEM_TABLE_REVISION)) {
		Print(L"    UEFI 2.0 supported\n\n");
	} else {
		DisplayErrorText(L"    UEFI 2.0 not supported!\n\n");
	}
	
	Print(L"    Using a screen resolution of %d x %d, mode %d.\n",
		numberOfDisplayRows, numberOfDisplayColoumns, highestModeNumberAvailable);
	Print(L"    Press any key to go back.");
	UINT64 key;
	key_read(&key, TRUE);
}

static int options_array[20];

#define OPTION(string, id) \
	if (options_array[id]) { \
		DisplayColoredText(string); \
	} else { \
		Print(string); \
	}

EFI_STATUS ConfigureKernel(CHAR16 *options, BOOLEAN preset_options[], int preset_options_length) {
	UINT64 key;
	EFI_STATUS err;
	
	StrCpy(options, L"");
	
	// Copy any options from the distribution's config entry into this string so
	// we can directly pass it to the kernel.
	StrCat(options, boot_options);
	
	// Copy everything from our preset options array into our options array.
	int i;
	for (i = 0; i < preset_options_length; i++) {
		options_array[i] = preset_options[i];
	}
	
	// Enter a loop where we show the menu.
	do {
		uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
		/*
		 * Configure the boot options to the Linux kernel. Let the user select any option
		 * that they think might facilitate booting Linux and add it to the options
		 * string once they press 0.
		 */
		Print(banner, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
		DisplayColoredText(L"\n    Configure Kernel Options:\n");
		Print(L"    Press the key corresponding to the number of the option to toggle.\n");
		OPTION(L"\n    1) nomodeset - Disable kernel mode setting.", 0);
		OPTION(L"\n    2) acpi=off - Disable ACPI.", 1);
		OPTION(L"\n    3) noefi - Disable EFI runtime services support.", 2);
		OPTION(L"\n    4) vga=ask - Show a menu of supported video modes.", 3);
		OPTION(L"\n    5) persistent - Make any changes to the flash storage persist.", 4);
		OPTION(L"\n    6) toram - Keep the entire distribution in RAM to minimize disk usage.", 5);
		OPTION(L"\n    7) debug - Enable kernel debugging.", 6);
		OPTION(L"\n    8) gpt - Forces disk with valid GPT signature but invalid Protective MBR" \
				" to be treated as GPT (useful for installing Linux on a Mac drive).", 7);
		Print(L"\n    9) Custom...");
		if (StrLen(options) > 0) Print(L" %s", options);

		Print(L"\n\n    0) Boot with selected options.\n");
		
		err = key_read(&key, TRUE);
		if (EFI_ERROR(err)) {
			Print(L"Error: could not read from keyboard: %d\n", err);
			return err;
		}
		
		UINT64 index = key - '0';
		options_array[index - 1] = !options_array[index - 1];
		
		// Allow the user to enter their own kernel parameter if they wish.
		if (index == 9) {
			uefi_call_wrapper(ST->ConOut->SetCursorPosition, 2, 0, numberOfDisplayRows - 1);
			uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, TRUE);
			Print(L"> ");
			
			CHAR16 *input = NULL;
			EFI_STATUS err = ReadStringFromKeyboard(&input);
			if (!EFI_ERROR(err)) StrCat(options, input);
			FreePool(input);
			
			uefi_call_wrapper(ST->ConOut->SetCursorPosition, 2, 0, 0);
			uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, FALSE);
		}
	} while(key != '0');
	
	// Now concatenate the individual options onto the option line.
	// I'm investigating a better way to do this.
	if (options_array[0]) {
		StrCat(options, L"nomodeset ");
	}
	
	if (options_array[1]) {
		StrCat(options, L"acpi=off ");
	}
	
	if (options_array[2]) {
		StrCat(options, L"noefi ");
	}
	
	if (options_array[3]) {
		StrCat(options, L"vga=ask ");
	}
	
	if (options_array[4]) {
		StrCat(options, L"persistent ");
	}
	
	if (options_array[5]) {
		StrCat(options, L"toram ");
	}
	
	if (options_array[6]) {
		StrCat(options, L"debug ");
	}
	
	if (options_array[7]) {
		StrCat(options, L"gpt ");
	}
	
	BootLinuxWithOptions(options, distribution_id);
	
	// Shouldn't get here unless something went wrong with the boot process.
	uefi_call_wrapper(BS->Stall, 1, 3 * 1000);
	uefi_call_wrapper(RT->ResetSystem, 4, EfiResetCold, EFI_SUCCESS, 0, NULL);
	return EFI_LOAD_ERROR;
}
