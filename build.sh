#! /bin/sh
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
# Copyright (C) 2013-2014 SevenBits
#
#
if make -C src >> /dev/null
then
	mkdir bin >> /dev/null 2> /dev/null # Make a new folder if we need to.
	mv src/enterprise.efi bin/bootX64.efi
	make -C src clean >> /dev/null 2> /dev/null
	make -C src/installer  >> /dev/null
	mv src/installer/install-enterprise bin/install-enterprise
	echo Done building!
	return 0
else
	return 1
fi
