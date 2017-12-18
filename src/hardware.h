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

#pragma once
#ifndef _hardware_h
#define _hardware_h

extern UINTN numberOfDisplayRows, numberOfDisplayColumns, highestModeNumberAvailable;

EFI_STATUS key_read(UINT64 *key, BOOLEAN wait);
EFI_STATUS SetupDisplay(VOID);
EFI_STATUS console_text_mode(VOID);

#endif
