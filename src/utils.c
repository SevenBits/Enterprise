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

#include "utils.h"

#ifdef __APPLE__
	#pragma mark - Get/Set/Delete EFI variables
#endif
EFI_STATUS efi_set_variable(const EFI_GUID *vendor, CHAR16 *name, CHAR8 *buf, UINTN size, BOOLEAN persistent) {
	UINT32 flags;
	
	flags = EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS;
	if (persistent) {
		flags |= EFI_VARIABLE_NON_VOLATILE;
	}
	
	return uefi_call_wrapper(RT->SetVariable, 5, name, (EFI_GUID *)vendor, flags, size, buf);
}

EFI_STATUS efi_delete_variable(const EFI_GUID *vendor, CHAR16 *name) {
	UINT32 flags;
	UINTN size = 0;
	
	flags = EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS;
	return uefi_call_wrapper(RT->SetVariable, 5, name, (EFI_GUID *)vendor, flags, size, NULL);
}

EFI_STATUS efi_get_variable(const EFI_GUID *vendor, CHAR16 *name, CHAR8 **buffer, UINTN *size) {
	CHAR8 *buf;
	UINTN length;
	EFI_STATUS err;

	length = sizeof(CHAR16 *) * EFI_MAXIMUM_VARIABLE_SIZE;
	buf = AllocatePool(length);
	if (!buf) {
		return EFI_OUT_OF_RESOURCES;
	}

	err = uefi_call_wrapper(RT->GetVariable, 5, name, (EFI_GUID *)vendor, NULL, &length, buf);
	if (!EFI_ERROR(err)) {
		*buffer = buf;
		if (size) {
			*size = length;
		}
	} else {
		FreePool(buf);
	}
	
	return err;
}

#ifdef __APPLE__
	#pragma mark - Text output functions
#endif
VOID DisplayColoredText(CHAR16 *string) {
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_YELLOW|EFI_BACKGROUND_BLACK);
	Print(string);
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK);
}

VOID DisplayErrorText(CHAR16 *string) {
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_RED|EFI_BACKGROUND_BLACK);
	Print(string);
	uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK);
}

#ifdef __APPLE__
	#pragma mark - Text input functions
#endif
EFI_STATUS ReadStringFromKeyboard(OUT CHAR16 **outString) {
	/* We allocate the memory in this function; it should not be allocated prior to being
	 * called. */
	EFI_STATUS err = EFI_SUCCESS;
	const UINT32 inputLength = 1024;
	UINT32 charactersEntered = 0;
	*outString = AllocateZeroPool(sizeof(CHAR16) * (inputLength + 1));
	
	CHAR16 key = 0;
	while (key != 13) {
		EFI_INPUT_KEY inputKey;
		err = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &inputKey);
		if (err != EFI_NOT_READY) {
			key = inputKey.UnicodeChar;
			
			if (!(key < 0x20 || key > 127)) {
				CHAR16 tempStr[2] = {key, '\0'};
				StrCat(*outString, tempStr);
				charactersEntered++;
				
				Print(L"%s", tempStr);
			}
		}
		
		// The user can't overflow the input buffer.
		if (charactersEntered == inputLength) break;
	}
	
	if (StrLen(*outString) > 0) StrCat(*outString, L" ");
	Print(L"\n");
	return err;
}

#ifdef __APPLE__
	#pragma mark - Character conversion functions missing from GNU-EFI
#endif
CHAR8* strcpya(CHAR8 *target, const CHAR8 *source) {
	while ((*target++ = *source++));
	return target;
}

CHAR8* strncpya(CHAR8 *target, const CHAR8 *source, INTN n) {
	INTN i = 0;

	// Copy all of the characters in the string up to the desired length.
	for (i = 0; i < n && source[i] != '\0'; i++) {
		target[i] = source[i];
	}

	// Pad the remainder of the string with null characters per the standard.
	for ( ; i < n; i++) {
		target[i] = '\0';
	}

	return target;
}

CHAR8* strcata(CHAR8 *dest, const CHAR8 *src) {
	CHAR8 *rdest = dest;
	
	while (*dest) {
		dest++;
	}
	
	while ((*dest++ = *src++));
	return rdest;
}

CHAR8* strchra(const CHAR8 *s, int c) {
	while (*s != (char)c) {
		if (!*s++) {
			return NULL;
		}
	}
	
	return (CHAR8 *)s;
}

INTN strposa(const CHAR8 *str, char c) {
	CHAR8 *p = strchra(str, c);
	return p ? p - str : -1;
}

CHAR8* UTF16toASCII(CHAR16 *InString, UINTN InLength) {
	CHAR8 *OutString, *InAs8;
	UINTN i = 0;
	
	OutString = AllocateZeroPool(InLength * sizeof(CHAR8));
	InAs8 = (CHAR8*)InString;
	while ((InAs8[i * 2] != '\0') && (i < InLength)) {
		OutString[i] = InAs8[i * 2];
		i++;
	}
	return OutString;
}

CHAR16* ASCIItoUTF16(CHAR8 *InString, UINTN InLength) {
	UINTN strlen = 0, i = 0;
	CHAR16 *str;

	str = AllocatePool((InLength + 1) * sizeof(CHAR16));
	while (i < InLength) {
		INTN utf8len;

		utf8len = NarrowToLongCharConvert(InString + i, str + strlen);
		if (utf8len <= 0) {
			i++;
			continue;
		}

		strlen++;
		i += utf8len;
	}
	str[strlen] = '\0';
	return str;
}

INTN NarrowToLongCharConvert(CHAR8 *InString, CHAR16 *c) {
	CHAR16 unichar;
	UINTN len;
	UINTN i;

	if (InString[0] < 0x80) {
		len = 1;
	} else if ((InString[0] & 0xe0) == 0xc0) {
		len = 2;
	} else if ((InString[0] & 0xf0) == 0xe0) {
		len = 3;
	} else if ((InString[0] & 0xf8) == 0xf0) {
		len = 4;
	} else if ((InString[0] & 0xfc) == 0xf8) {
		len = 5;
	} else if ((InString[0] & 0xfe) == 0xfc) {
		len = 6;
	} else {
		return -1;
	}

	switch (len) {
		case 1:
			unichar = InString[0];
			break;
		case 2:
			unichar = InString[0] & 0x1f;
			break;
		case 3:
			unichar = InString[0] & 0x0f;
			break;
		case 4:
			unichar = InString[0] & 0x07;
			break;
		case 5:
			unichar = InString[0] & 0x03;
			break;
		case 6:
			unichar = InString[0] & 0x01;
			break;
	}

	for (i = 1; i < len; i++) {
		if ((InString[i] & 0xc0) != 0x80) {
			return -1;
		}
		
		unichar <<= 6;
		unichar |= InString[i] & 0x3f;
	}

	*c = unichar;
	return len;
}

/**
 * Converts between different path formats. This is a very rudimentary search and does not work
 * correctly if there is more than one type of path separator in a string.
 */
CHAR8* PathConvert(CHAR8 needle, CHAR8 *path) {
	// Iterate through path, replacing all path separators with needle.
	for (INTN i = 0; path[i] != '\0'; i++) {
		if (path[i] == '/' || path[i] == '\\' || path[i] == ':') {
			path[i] = needle;
		}
	}
	
	return path;
}

BOOLEAN FileExists(EFI_FILE_HANDLE dir, CHAR16 *name) {
	EFI_FILE_HANDLE handle;
	EFI_STATUS err;

	err = uefi_call_wrapper(dir->Open, 5, dir, &handle, name, EFI_FILE_MODE_READ, NULL);
	if (EFI_ERROR(err)) {
		goto out;
	}

	uefi_call_wrapper(handle->Close, 1, handle);
	return TRUE;
out:
	return FALSE;
}

#ifdef __APPLE__
	#pragma mark - Functions for reading and parsing config files.
#endif
UINTN FileRead(EFI_FILE_HANDLE dir, const CHAR16 *name, CHAR8 **content) {
	EFI_FILE_HANDLE handle;
	EFI_FILE_INFO *info;
	CHAR8 *buf;
	UINTN buflen;
	EFI_STATUS err;
	UINTN len = 0;
	
	err = uefi_call_wrapper(dir->Open, 5, dir, &handle, name, EFI_FILE_MODE_READ, NULL);
	if (EFI_ERROR(err)) {
		goto out;
	}
	
	info = LibFileInfo(handle);
	buflen = info->FileSize+1;
	buf = AllocatePool(buflen);
	
	err = uefi_call_wrapper(handle->Read, 3, handle, &buflen, buf);
	if (EFI_ERROR(err) == EFI_SUCCESS) {
		buf[buflen] = '\0';
		*content = buf;
		len = buflen;
	} else {
		FreePool(buf);
	}
	
	FreePool(info);
	uefi_call_wrapper(handle->Close, 1, handle);
out:
	return len;
}

// This code has been adapted from gummiboot. Thanks, guys!
CHAR8* GetConfigurationKeyAndValue(CHAR8 *content, UINTN *pos, CHAR8 **key_ret, CHAR8 **value_ret) {
	CHAR8 *line;
	UINTN linelen;
	CHAR8 *value;

skip:
	line = content + *pos;
	if (*line == '\0') {
		return NULL;
	}

	linelen = 0;
	while (line[linelen] && !strchra((CHAR8 *)"\n\r", line[linelen])) {
		linelen++;
	}

	/* Move the position to the next line. */
	*pos += linelen;
	if (content[*pos]) {
		(*pos)++;
	}

	/* The line has no length, so it is empty. */
	if (linelen == 0) {
		goto skip;
	}

	/* Add a string terminator character. */
	line[linelen] = '\0';

	/* Remove leading and trailing whitespace. */
	while (strchra((CHAR8 *)" \t", *line)) {
		line++;
		linelen--;
	}

	while (linelen > 0 && strchra((CHAR8 *)" \t", line[linelen-1])) {
		linelen--;
	}
	line[linelen] = '\0';

	if (*line == '#') {
		goto skip;
	}

	/* Split the key and the value. */
	value = line;
	while (*value && !strchra((CHAR8 *)" \t", *value)) {
		value++;
	}
	
	if (*value == '\0') {
		goto skip;
	}
	
	*value = '\0';
	value++;
	while (*value && strchra((CHAR8 *)" \t", *value)) {
		value++;
	}

	*key_ret = line;
	*value_ret = value;
	return line;
}
