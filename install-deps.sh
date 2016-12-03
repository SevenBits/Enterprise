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
# Copyright (C) 2014 SevenBits
#
#
set -e
mkdir build-deps 2> /dev/null >> /dev/null
cd build-deps
wget -q https://downloads.sourceforge.net/project/gnu-efi/gnu-efi-3.0.3.tar.bz2
tar -jxvf gnu-efi-3.0.3.tar.bz2 >> /dev/null

cd gnu-efi-3.0.3
make
sudo make install

cd ../..
rm -rf build-deps

echo Done!
