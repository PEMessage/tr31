/**
 * @file tr31-kcv.c
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

#include "crypto_tdes.h"
#include "crypto_aes.h"
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
struct tr31_kcv_options_t {
	bool found_stdin_arg;
	bool key;

	// key parameters
	// valid if key is true
	size_t key_buf_len;
	uint8_t key_buf[32]; // max 256-bit key

	const char* algorithm; // TDES or AES
	const char* kcv_algorithm; // legacy or cmac
};

// helper functions
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state);
static void* read_file(FILE* file, size_t* len);
static int parse_hex(const char* hex, void* bin, size_t bin_len);
static void print_hex(const void* buf, size_t length);

// argp option keys
enum tr31_kcv_option_keys_t {
	TR31_KCV_OPTION_KEY = -255, // negative value to avoid short options
	TR31_KCV_OPTION_ALGORITHM,
	TR31_KCV_OPTION_KCV_ALGORITHM,
	TR31_KCV_OPTION_VERSION,
};

// argp option structure
static struct argp_option argp_options[] = {
	{ "key", TR31_KCV_OPTION_KEY, "KEY", 0, "Key for which to compute the Key Check Value (KCV). Use - to read raw bytes from stdin." },
	{ "algorithm", TR31_KCV_OPTION_ALGORITHM, "TDES|AES", 0, "Algorithm of the key." },
	{ "kcv-algorithm", TR31_KCV_OPTION_KCV_ALGORITHM, "legacy|cmac", 0, "KCV algorithm. Defaults to \"legacy\" for TDES and \"cmac\" for AES. AES only supports \"cmac\"." },

	{ "version", TR31_KCV_OPTION_VERSION, NULL, 0, "Display TR-31 library version" },

	{ 0 },
};

// argp configuration
static struct argp argp_config = {
	argp_options,
	argp_parser_helper,
	NULL,
	" \v" // force the text to be after the options in the help message
	"Compute the Key Check Value (KCV) of a key, as used by the KC, KP and PK "
	"optional blocks.\n\n"
	"NOTE:\nThe KEY value is a string of hex digits representing binary data, or - to read raw bytes from stdin.",
};

// argp parser helper function
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state)
{
	int r;
	struct tr31_kcv_options_t* options;
	void* buf = NULL;
	size_t buf_len = 0;

	options = state->input;
	if (!options) {
		return ARGP_ERR_UNKNOWN;
	}

	if (arg) {
		// Process KEY argument
		switch (key) {
			case TR31_KCV_OPTION_KEY: {
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
		case TR31_KCV_OPTION_KEY:
			if (buf_len > sizeof(options->key_buf)) {
				argp_error(state, "KEY string may not have more than %zu digits (thus %zu bytes)",
					sizeof(options->key_buf) * 2,
					sizeof(options->key_buf)
				);
				free(buf);
				return EINVAL;
			}
			memcpy(options->key_buf, buf, buf_len);
			options->key_buf_len = buf_len;
			options->key = true;

			free(buf);
			buf = NULL;

			return 0;

		case TR31_KCV_OPTION_ALGORITHM:
			options->algorithm = arg;
			return 0;

		case TR31_KCV_OPTION_KCV_ALGORITHM:
			options->kcv_algorithm = arg;
			return 0;

		case TR31_KCV_OPTION_VERSION: {
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
			if (!options->key) {
				argp_error(state, "The --key option is required");
				return EINVAL;
			}
			if (!options->algorithm) {
				argp_error(state, "The --algorithm option is required");
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

// KCV computation helper function
static int do_tr31_kcv(const struct tr31_kcv_options_t* options)
{
	int r;
	uint8_t kcv[AES_KCV_SIZE]; // large enough for all supported KCV algorithms
	size_t kcv_len;
	const char* kcv_algorithm;

	if (strcmp(options->algorithm, "TDES") == 0) {
		kcv_algorithm = options->kcv_algorithm ? options->kcv_algorithm : "legacy";

		if (strcmp(kcv_algorithm, "legacy") == 0) {
			kcv_len = DES_KCV_SIZE_LEGACY;
			r = crypto_tdes_kcv_legacy(options->key_buf, options->key_buf_len, kcv);
		} else if (strcmp(kcv_algorithm, "cmac") == 0) {
			kcv_len = DES_KCV_SIZE_CMAC;
			r = crypto_tdes_kcv_cmac(options->key_buf, options->key_buf_len, kcv);
		} else {
			fprintf(stderr, "Unsupported KCV algorithm \"%s\"\n", kcv_algorithm);
			return 1;
		}

	} else if (strcmp(options->algorithm, "AES") == 0) {
		kcv_algorithm = options->kcv_algorithm ? options->kcv_algorithm : "cmac";

		if (strcmp(kcv_algorithm, "cmac") == 0) {
			kcv_len = AES_KCV_SIZE;
			r = crypto_aes_kcv(options->key_buf, options->key_buf_len, kcv);
		} else {
			fprintf(stderr, "Unsupported KCV algorithm \"%s\" for AES\n", kcv_algorithm);
			return 1;
		}

	} else {
		fprintf(stderr, "%s\n", tr31_get_error_string(TR31_ERROR_UNSUPPORTED_ALGORITHM));
		return 1;
	}

	if (r) {
		fprintf(stderr, "KCV computation failed; error %d: %s\n", r, tr31_get_error_string(r));
		return 1;
	}

	printf("KCV: ");
	print_hex(kcv, kcv_len);
	printf("\n");

	return 0;
}

int main(int argc, char** argv)
{
	int r;
	struct tr31_kcv_options_t options;

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

	r = do_tr31_kcv(&options);

exit:
	// Cleanup
	if (options.key) {
		crypto_cleanse(options.key_buf, sizeof(options.key_buf));
	}

	return r;
}
