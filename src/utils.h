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
#ifndef _utils_h
#define _utils_h
#include "main.h"

EFI_STATUS efi_set_variable(const EFI_GUID *, CHAR16 *, CHAR8 *, UINTN, BOOLEAN);
EFI_STATUS efi_delete_variable(const EFI_GUID *, CHAR16 *);
EFI_STATUS efi_get_variable(const EFI_GUID *, CHAR16 *, CHAR8 **, UINTN *);

CHAR8* strcpya(CHAR8 *, const CHAR8 *);
CHAR8* strchra(const CHAR8 *, int);
CHAR8* strncpya(CHAR8 *, const CHAR8 *, INTN);
CHAR8* strcata(CHAR8 *, const CHAR8 *);
INTN strposa(const CHAR8 *, char);

INTN NarrowToLongCharConvert(CHAR8 *InChar, OUT CHAR16 *);
CHAR8* PathConvert(CHAR8, CHAR8 *);
CHAR16* ASCIItoUTF16(CHAR8 *, UINTN);
CHAR8* UTF16toASCII(CHAR16 *, UINTN);

BOOLEAN FileExists(EFI_FILE_HANDLE, CHAR16 *);
UINTN FileRead(EFI_FILE_HANDLE, const CHAR16 *, CHAR8 **);
CHAR8* GetConfigurationKeyAndValue(CHAR8 *, UINTN *, CHAR8 **, CHAR8 **);
VOID DisplayColoredText(CHAR16 *);
VOID DisplayErrorText(CHAR16 *);
EFI_STATUS ReadStringFromKeyboard(OUT CHAR16 **);

#endif
