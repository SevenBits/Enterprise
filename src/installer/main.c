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
 * Copyright (C) 2015 SevenBits
 *
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <sys/stat.h>

static char *install_path = NULL;
static char *config_path = NULL;
static char *program_name = NULL; // Stores the name the program was invoked with

// The locations that we search for in looking for the
// Enterprise files (i.e those ending in *.efi)
// Should end with NULL
static const char *search_paths[6] = {"/etc/enterprise-efi/resources/", "/etc/opt/enterprise-efi/resources/", "/usr/share/enterprise-efi/resources/", "resources/", "", NULL};

static bool should_verify;
static bool should_configure = true; // Whether or not to write data to the config file

static void usage(char *prog_name) {
	printf("usage: %s [--verify] [--help] [--blank | --config file] path\n", prog_name);
	printf("\t--verify\tVerify that the installation is configured properly after setup\n");
	printf("\t--blank\t\tWrite an empty configuration file\n");
	printf("\t--config file\tSpecifies the path to the configuration file\n");
	printf("\t--help\t\tShows this help message\n");
	printf("\tpath\t\tThe path to the mount point of the volume on which to install Enterprise\n");
	printf("NOTE: you must specify either --blank or --config.\n");
}

static bool handle_option(char *option, char **arg_ptr) {
	if (strcmp("--verify", option) == 0) {
		should_verify = true;
		return true;
	} else if (strcmp("--blank", option) == 0) {
		if (config_path) {
			fprintf(stderr, "Error: --blank and --config are mutually exclusive.\n");
			usage(program_name);
			exit(1);
		}

		should_configure = false;
		return true;
	} else if (strcmp("--config", option) == 0) {
		if (*(arg_ptr + 1) != 0) {
			if (**(arg_ptr + 1) == '-') {
				fprintf(stderr, "Error: --config expects argument\n");
				usage(program_name);
				exit(1);
			}
			
			if (!should_configure) {
				fprintf(stderr, "Error: --blank and --config are mutually exclusive.\n");
				usage(program_name);
				exit(1);
			}
			
			if (config_path) free(config_path);
			
			config_path = malloc(sizeof(char) * strlen(*(arg_ptr + 1)));
			if (!config_path) {
				fprintf(stderr, "Error: failed to allocate memory\n");
				exit(1);
			}
			
			strcpy(config_path, *(arg_ptr + 1));
			arg_ptr++;
		} else {
			fprintf(stderr, "Error: --config expects argument\n");
			usage(program_name);
			exit(1);
		}

		return true;
	} else if (strcmp("--help", option) == 0) {
		usage(program_name);
		exit(0);
	}
	
	return false;
}

static void parse_args(int argc, char **argv) {
	if (argc < 2) {
		usage(*argv);
		exit(1);
	}
	
	char **arg_ptr = argv + 1;
	while (arg_ptr != argv + argc) {
		if (**arg_ptr == '-') {
			// Parse an option.
			bool result = handle_option(*arg_ptr, arg_ptr);
			if (!result) {
				fprintf(stderr, "Error: unrecognized option: %s. Aborting.\n", *arg_ptr);
				exit(1);
			}
		} else {
			// We have the user's desired path
			if (install_path) free(install_path);
			
			install_path = malloc(sizeof(char) * strlen(*arg_ptr));
			if (!install_path) {
				fprintf(stderr, "Error: failed to allocate memory. Aborting.\n");
				exit(1);
			}
			
			if (strlen(*arg_ptr) > 0) strcpy(install_path, *arg_ptr);
		}
		
		arg_ptr++; // Move and parse the next argument.
	}
}

static bool is_directory(const char *path) {
	struct stat s;
	if (stat(path, &s) == 0) {
		if (s.st_mode & S_IFDIR) {
			return true;
		}
	}
	
	return false;
}

static const char* check_search_path(void) {
	// Try each listing in the list of potential Enterprises sources
	// to find one with the valid files.
	const char **needle = search_paths;
	while (*needle) {
		bool found = false;
		char *desired_file_name = "bootX64.efi";

		char *tmp = malloc(strlen(*needle) + strlen(desired_file_name) + 1);
		strcpy(tmp, *needle);
		strcat(tmp, desired_file_name);

		FILE *fp;
		if ((fp = fopen(tmp, "rb"))) {
			found = true;
			fclose(fp);
		}

		free(tmp);
		if (!found) needle++;
		else return *needle;
	}
	
	return NULL;
}

static bool copy_file(const const char*, const const char*) __attribute__((nonnull (1,2)));
static bool copy_file(const const char *source, const const char *destination) {
	char *buffer;
	FILE *inFilePointer = fopen(source, "rb");
	FILE *outFilePointer = fopen(destination, "wb");
	
	fseek(inFilePointer, 0, SEEK_END);
	long fsize = ftell(inFilePointer);
	rewind(inFilePointer);
	
	buffer = malloc(fsize + 1);
	if (!buffer) goto no_memory;
	fread(buffer, fsize, 1, inFilePointer);
	fclose(inFilePointer);
	
	*(buffer + fsize) = 0;
	
	if (fwrite(buffer, sizeof(char), fsize, outFilePointer) != fsize)
		goto write_failed;

	fclose(outFilePointer);
	free(buffer);
	return true;

no_memory:
	fprintf(stderr, "Error: failed to allocate memory. Aborting.\n");
	fclose(outFilePointer);
	return false;

write_failed:
	fprintf(stderr, "Error: failed to copy %s. Aborting.\n", source);
	fclose(outFilePointer);
	return false;
}

static bool perform_setup(void) {
	// Get ready to copy the necessary files to the chosen path.
	if (!install_path) {
		usage(program_name);
		exit(1);
	}

	if (!is_directory(install_path)) {
		fprintf(stderr, "Error: %s is not a directory. Aborting.\n", install_path);
		return false;
	}
	
	const char *support_path = check_search_path();
	if (!support_path) {
		fprintf(stderr, "Error: can't find required support files. Aborting.\n");
		return false;
	}
	
	// If install_path doesn't end with a / character, bad things will happen.
	// Therefore, we need to add one if that's the case.
	{
		int len = strlen(install_path);
		if (*(install_path + len - 1) != '/') {
			void *temp = realloc(install_path, len + 2);
			if (temp) {
				install_path = temp;
				*(install_path + len) = '/';
			} else goto no_memory;
		}
	}
	
	// Write the configuration file (if desired)
	const char *configuration_file_name = "enterprise.cfg";
	char *full_config_path = malloc(strlen(install_path) + strlen(configuration_file_name) + 1);
	if (!full_config_path) goto no_memory;
	
	strcpy(full_config_path, install_path);
	strcat(full_config_path, configuration_file_name);
	printf("%s, should_configure = %s\n", full_config_path, should_configure ? "true" : "false");

	if (!should_configure) {
		FILE *fp = fopen(full_config_path, "w");
		if (fp) fclose(fp);
		else goto no_config_written;
	} else {
		if (!copy_file(config_path, full_config_path)) return false;
	}

	return true;
	
no_memory:
	fprintf(stderr, "Error: failed to allocate memory. Aborting.\n");
	return false;
no_config_written:
	fprintf(stderr, "Error: failed to write configuration file. Aborting.\n");
	return false;
}

int main(int argc, char **argv) {
	// Handle command line arguments
	program_name = strdup(*argv);
	parse_args(argc, argv);
	if (!perform_setup()) {
		return 1;
	}
	
	free(install_path);
	free(config_path);
	free(program_name);
}
