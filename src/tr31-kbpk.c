/**
 * @file tr31-kbpk.c
 *
 * Copyright 2026 Leon Lynch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "tr31.h"
#include "tr31_config.h"
#include "tr31_crypto.h"

#include "crypto_mem.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <stdlib.h>
#include <stdio.h>
#include <argp.h>

#include <ctype.h> // for isxdigit

#ifdef _WIN32
// for _setmode
#include <fcntl.h>
#include <io.h>
#endif

// command line options
struct tr31_kbpk_options_t {
	bool found_stdin_arg;
	bool kbpk;

	// kbpk parameters
	// valid if kbpk is true
	size_t kbpk_buf_len;
	uint8_t kbpk_buf[32]; // max 256-bit KBPK
};

// helper functions
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state);
static void* read_file(FILE* file, size_t* len);
static int parse_hex(const char* hex, void* bin, size_t bin_len);
static void print_hex(const void* buf, size_t length);

// argp option keys
enum tr31_kbpk_option_keys_t {
	TR31_KBPK_OPTION_KBPK = -255, // negative value to avoid short options
	TR31_KBPK_OPTION_VERSION,
};

// argp option structure
static struct argp_option argp_options[] = {
	{ "kbpk", TR31_KBPK_OPTION_KBPK, "KEY", 0, "Key block protection key. Use - to read raw bytes from stdin." },

	{ "version", TR31_KBPK_OPTION_VERSION, NULL, 0, "Display TR-31 library version" },

	{ 0 },
};

// argp configuration
static struct argp argp_config = {
	argp_options,
	argp_parser_helper,
	NULL,
	" \v" // force the text to be after the options in the help message
	"Derive and print the intermediate keys, being the key block encryption key "
	"(KBEK) and key block authentication key (KBAK), for key block format "
	"versions A, B, C, D and E from the provided key block protection key (KBPK).\n\n"
	"NOTE:\nThe KEY value is a string of hex digits representing binary data, or - to read raw bytes from stdin.",
};

// argp parser helper function
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state)
{
	int r;
	struct tr31_kbpk_options_t* options;
	void* buf = NULL;
	size_t buf_len = 0;

	options = state->input;
	if (!options) {
		return ARGP_ERR_UNKNOWN;
	}

	if (arg) {
		// Process KEY argument
		switch (key) {
			case TR31_KBPK_OPTION_KBPK: {
				// If argument is "-", read from stdin
				if (strcmp(arg, "-") == 0) {
					if (options->found_stdin_arg) {
						argp_error(state, "Only one option may be read from stdin");
						return EINVAL;
					}
					options->found_stdin_arg = true;

					buf = read_file(stdin, &buf_len);
					if (!buf) {
						argp_error(state, "Failed to read data from stdin");
						return EINVAL;
					}

				} else {
					// Parse KEY argument as hex data
					size_t arg_len = strlen(arg);
					if (arg_len % 2 != 0) {
						argp_error(state, "KEY string must have even number of digits");
						return EINVAL;
					}
					buf_len = arg_len / 2;
					buf = malloc(buf_len);
					if (!buf) {
						argp_error(state, "Memory allocation failed");
						return ENOMEM;
					}

					r = parse_hex(arg, buf, buf_len);
					if (r) {
						argp_error(state, "KEY string must consist of hex digits");
						free(buf);
						return EINVAL;
					}
				}

				break;
			}
		}
	}

	switch (key) {
		case TR31_KBPK_OPTION_KBPK:
			if (buf_len > sizeof(options->kbpk_buf)) {
				argp_error(state, "KEY string may not have more than %zu digits (thus %zu bytes)",
					sizeof(options->kbpk_buf) * 2,
					sizeof(options->kbpk_buf)
				);
				free(buf);
				return EINVAL;
			}
			memcpy(options->kbpk_buf, buf, buf_len);
			options->kbpk_buf_len = buf_len;
			options->kbpk = true;

			free(buf);
			buf = NULL;

			return 0;

		case TR31_KBPK_OPTION_VERSION: {
			const char* version;

			version = tr31_lib_version_string();
			if (version) {
				printf("%s\n", version);
			} else {
				printf("Unknown\n");
			}
			exit(EXIT_SUCCESS);
			return 0;
		}

		case ARGP_KEY_END: {
			// check for required options
			if (!options->kbpk) {
				argp_error(state, "The --kbpk option is required");
				return EINVAL;
			}

			return 0;
		}

		default:
			return ARGP_ERR_UNKNOWN;
	}
}

// File/stdin read helper function
static void* read_file(FILE* file, size_t* len)
{
	const size_t block_size = 4096; // Use common page size
	void* buf = NULL;
	size_t buf_len = 0;
	size_t total_len = 0;

	if (!file) {
		*len = 0;
		return NULL;
	}

#ifdef _WIN32
	_setmode(_fileno(file), _O_BINARY);
#endif

	do {
		// Grow buffer
		buf_len += block_size;
		buf = realloc(buf, buf_len);

		// Read next block
		total_len += fread(buf + total_len, 1, block_size, file);
		if (ferror(file)) {
			free(buf);
			*len = 0;
			return NULL;
		}
	} while (!feof(file));

	*len = total_len;
	return buf;
}

// hex parser helper function
static int parse_hex(const char* hex, void* bin, size_t bin_len)
{
	size_t hex_len = bin_len * 2;

	for (size_t i = 0; i < hex_len; ++i) {
		if (!isxdigit(hex[i])) {
			return -1;
		}
	}

	while (*hex && bin_len--) {
		uint8_t* ptr = bin;

		char str[3];
		strncpy(str, hex, 2);
		str[2] = 0;

		*ptr = strtoul(str, NULL, 16);

		hex += 2;
		++bin;
	}

	return 0;
}

// hex output helper function
static void print_hex(const void* buf, size_t length)
{
	const uint8_t* ptr = buf;
	for (size_t i = 0; i < length; i++) {
		printf("%02X", ptr[i]);
	}
}

// intermediate key output helper function
static void print_kbxk(int r, const void* kbek, const void* kbak, size_t kbxk_len)
{
	if (r) {
		printf("\tNot applicable: %s\n", tr31_get_error_string(r));
		return;
	}

	printf("\tKBEK: ");
	print_hex(kbek, kbxk_len);
	printf("\n");
	printf("\tKBAK: ");
	print_hex(kbak, kbxk_len);
	printf("\n");
}

// intermediate key derivation helper function
static void do_tr31_kbpk(const struct tr31_kbpk_options_t* options)
{
	int r;
	uint8_t kbek[AES256_KEY_SIZE]; // large enough for all supported KBPK lengths
	uint8_t kbak[AES256_KEY_SIZE];
	size_t kbxk_len = options->kbpk_buf_len;

	printf("KBPK: ");
	print_hex(options->kbpk_buf, options->kbpk_buf_len);
	printf(" (%zu bytes)\n\n", options->kbpk_buf_len);

	// Versions A and C use the TDES Key Variant Binding Method
	printf("Version A (TDES Key Variant Binding Method):\n");
	r = tr31_tdes_kbpk_variant(options->kbpk_buf, options->kbpk_buf_len, kbek, kbak);
	print_kbxk(r, kbek, kbak, kbxk_len);

	// Version B uses the TDES Key Derivation Binding Method
	printf("Version B (TDES Key Derivation Binding Method):\n");
	r = tr31_tdes_kbpk_derive(options->kbpk_buf, options->kbpk_buf_len, kbek, kbak);
	print_kbxk(r, kbek, kbak, kbxk_len);

	printf("Version C (TDES Key Variant Binding Method):\n");
	r = tr31_tdes_kbpk_variant(options->kbpk_buf, options->kbpk_buf_len, kbek, kbak);
	print_kbxk(r, kbek, kbak, kbxk_len);

	// Version D uses the AES Key Derivation Binding Method in CBC mode
	printf("Version D (AES Key Derivation Binding Method):\n");
	r = tr31_aes_kbpk_derive(options->kbpk_buf, options->kbpk_buf_len, TR31_AES_MODE_CBC, kbek, kbak);
	print_kbxk(r, kbek, kbak, kbxk_len);

	// Version E uses the AES Key Derivation Binding Method in CTR mode
	printf("Version E (AES Key Derivation Binding Method):\n");
	r = tr31_aes_kbpk_derive(options->kbpk_buf, options->kbpk_buf_len, TR31_AES_MODE_CTR, kbek, kbak);
	print_kbxk(r, kbek, kbak, kbxk_len);

	crypto_cleanse(kbek, sizeof(kbek));
	crypto_cleanse(kbak, sizeof(kbak));
}

int main(int argc, char** argv)
{
	int r;
	struct tr31_kbpk_options_t options;

	memset(&options, 0, sizeof(options));

	if (argc == 1) {
		// No command line options
		argp_help(&argp_config, stdout, ARGP_HELP_STD_HELP, argv[0]);
		return 1;
	}

	// parse command line options
	r = argp_parse(&argp_config, argc, argv, 0, 0, &options);
	if (r) {
		fprintf(stderr, "Failed to parse command line\n");
		goto exit;
	}

	do_tr31_kbpk(&options);

exit:
	// Cleanup
	if (options.kbpk) {
		crypto_cleanse(options.kbpk_buf, sizeof(options.kbpk_buf));
	}

	return r;
}
