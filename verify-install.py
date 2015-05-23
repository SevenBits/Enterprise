#! /usr/bin/env python2.7
#
# Tool intended to help facilitate the process of booting Linux on Intel
# Macintosh computers made by Apple from a USB stick or similar.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# Copyright (C) 2014 SevenBits
#
#
from sys import exit
from sys import argv
import argparse
import os
""" Note: this program assumes that it is being executed from the
 root directory of a USB drive containing an installation of
 Enterprise. It is possible to pass on the command line the drive
 to check, however.

 It verifies syntax and whether all required files are present."""
def main():
	"""The program's main method."""
	## Parse command line arguments.
	if len(argv) > 1:
		parser = argparse.ArgumentParser(description="Ensures the validity of an Enterprise installation")
		parser.add_argument("path", nargs=1, help="Path to the USB drive that you want to check (if ommitted, the program checks the current directory)")
		
		args = parser.parse_args()
		os.chdir(args.path[0])

	## Open file and setup variables.	
	installValid = True
	reason = ""

	configIsValid = verifyConfigurationFile("efi/boot/.MLUL-Live-USB") or verifyConfigurationFile("efi/boot/enterprise.cfg")
	## Check if the Enterprise files are present.
	if ( not fileExists("efi/boot/boot.efi") or
		 not fileExists("efi/boot/bootX64.efi")
		):
		reason = "One or more of the EFI boot files are not present."
		installValid = False
	elif (not fileExists("efi/boot/.MLUL-Live-USB")):
		reason = "No Enterprise configuration file present."
		installValid = False

	## Verify that the configuration file is present.
	elif (not configIsValid):
		reason = "The configuration file is bad."
		installValid = False

	## Display whether or not everything's good.
	if (installValid):
		print("The installation is valid. All's good. :)")
		exit(0)
	else:
		print("The installation is invalid: {0}".format(reason))
		exit(1)

def verifyConfigurationFile(file):
	"""Verify whether Enterprise's configuration file
		is valid."""
	validKeys = ["entry", "family", "kernel", "initrd", "root"]
	verifyIsValid = True
	if not (fileExists(file)):
		return "bad: the file does not exist"

	## Open the file.
	file = open(file, 'r')

	## Read each line.
	lineNumber = 1
	n = 1 # We must read at least one byte.
	while True:
		chunk = file.read(n)
		if (chunk == ''):
			break
		else:
			line = chunk + file.readline()
			if (chunk == os.linesep):
				continue
			
			didSucceed, message = processConfigLine(line, validKeys)
			if not (didSucceed):
				print("Syntax error on line {0}: {1}".format(lineNumber, message))
				verifyIsValid = False
		lineNumber += 1
	return verifyIsValid
			
def processConfigLine(line, validKeys):
	"""Determine whether a configuration file's line starts
		with valid keys."""
	command = line.split(' ')[0]
	message = ""
	if (command.startswith('#')):
		return (True, message)
	keyIsValid = command in validKeys
	if not (keyIsValid):
		message = "key \"{0}\" is not valid".format(command)

	value = (keyIsValid, message)
	return value

def fileExists(file):
	"""Check if a file exists."""
	try:
		open(file).close()
		return True
	except IOError:
		return False

if  __name__ == '__main__':
    main()
