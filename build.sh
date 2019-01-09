#!/bin/sh
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
# Copyright (C) 2013-2019 SevenBits

# Check if we have Docker installed. If we do, perform the build in a
# container so we don't pollute the system with EFI libraries.
which docker >/dev/null
HAVE_DOCKER=$?
DOCKER_TAG=sevenbits:efi-build
if [ $HAVE_DOCKER -eq 0 ]; then
	echo "Found Docker at path: $(which docker)"
	echo "Using Docker to perform build."
	if [ "$(docker images -q $DOCKER_TAG 2> /dev/null)" = "" ]; then
		echo "Building GNU-EFI Docker image..."
		docker build -t $DOCKER_TAG .
	fi

	docker run -it --name enterprise-build --rm -v `pwd`:/src:delegated -w /src $DOCKER_TAG ./build.sh
	exit $?
fi

# If Docker is not installed, then build the image as usual.
if make -C src >> /dev/null
then
	mkdir bin >> /dev/null 2> /dev/null # Make a new folder if we need to.
	mv src/enterprise.efi bin/bootX64.efi
	make -C src clean
	make -C src/installer  >> /dev/null
	mv src/installer/install-enterprise bin/install-enterprise
	echo Done building!
	exit 0
else
	exit 1
fi
