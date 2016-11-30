 #
 # Tool intended to help facilitate the process of booting Linux on Intel
 # Macintosh computers made by Apple from a USB stick or similar.
 #
 # This program is free software: you can redistribute it and/or modify
 # it under the terms of version 3 of the GNU General Public License as
 # published by the Free Software Foundation.
 #
 # This program is distributed in the hope that it will be useful, but
 # WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 # Lesser General Public License for more details.
 #
 # Copyright (C) 2013 SevenBits
 #
 #
ARCH            ?= $(shell uname -m | sed s,i[3456789]86,ia32,)

EFI-OBJS        = main.o menu.o utils.o distribution.o
TARGET          = enterprise.efi

EFIINC          = /usr/local/include/efi
EFIINCS         = -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol
LIB             = /usr/local/lib
EFILIB          = /usr/local/lib
EFI_CRT_OBJS    = $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS         = $(EFILIB)/elf_$(ARCH)_efi.lds

CFLAGS          = $(EFIINCS) -fno-stack-protector -std=c99 -fpic \
		  		-fshort-wchar -mno-red-zone -Wall -Wextra
ifeq ($(ARCH),x86_64)
  CFLAGS += -DEFI_FUNCTION_WRAPPER
endif

LDFLAGS         = -nostdlib -znocombreloc -T $(EFI_LDS) -shared \
		  -Bsymbolic -L $(EFILIB) -L $(LIB) $(EFI_CRT_OBJS) 

all: $(TARGET)

clean:
	rm *.o
	rm *.so

enterprise.so: $(EFI-OBJS)
	ld $(LDFLAGS) $(EFI-OBJS) -o $@ -lefi -lgnuefi

%.efi: %.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
		-j .dynsym  -j .rel -j .rela -j .reloc \
		--target=efi-app-$(ARCH) $^ $@
