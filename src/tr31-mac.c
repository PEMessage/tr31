/**
 * @file tr31-mac.c
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
struct tr31_mac_options_t {
	bool found_stdin_arg;
	bool kbak;
	bool have_version;
	bool have_data;
	bool verify;

	// key block authentication key parameters
	// valid if kbak is true
	size_t kbak_buf_len;
	uint8_t kbak_buf[32]; // max 256-bit KBAK

	// key block format version
	// valid if have_version is true
	unsigned int version;

	// input data over which the MAC is computed
	// valid if have_data is true
	void* data;
	size_t data_len;

	// MAC to verify
	// valid if verify is true
	uint8_t mac_verify[AES_CMAC_SIZE];
	size_t mac_verify_len;
};

// helper functions
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state);
static void* read_file(FILE* file, size_t* len);
static int parse_hex(const char* hex, void* bin, size_t bin_len);
static void print_hex(const void* buf, size_t length);

// argp option keys
enum tr31_mac_option_keys_t {
	TR31_MAC_OPTION_KBAK = -255, // negative value to avoid short options
	TR31_MAC_OPTION_VERSION_ID,
	TR31_MAC_OPTION_DATA,
	TR31_MAC_OPTION_VERIFY,
	TR31_MAC_OPTION_VERSION,
};

// argp option structure
static struct argp_option argp_options[] = {
	{ "kbak", TR31_MAC_OPTION_KBAK, "KEY", 0, "Key block authentication key (KBAK). Use - to read raw bytes from stdin. See tr31-kbpk." },
	{ "version-id", TR31_MAC_OPTION_VERSION_ID, "A|B|C|D|E", 0, "Key block format version, selecting the MAC algorithm: A/C use TDES CBC-MAC, B uses TDES CMAC, D/E use AES CMAC." },
	{ "data", TR31_MAC_OPTION_DATA, "DATA", 0, "Data over which the MAC is computed (typically the header and encrypted payload). Use - to read raw bytes from stdin." },
	{ "verify", TR31_MAC_OPTION_VERIFY, "MAC", 0, "Verify MAC instead of generating it." },

	{ "version", TR31_MAC_OPTION_VERSION, NULL, 0, "Display TR-31 library version" },

	{ 0 },
};

// argp configuration
static struct argp argp_config = {
	argp_options,
	argp_parser_helper,
	NULL,
	" \v" // force the text to be after the options in the help message
	"Generate or verify the key block authenticator (MAC) for a given key block "
	"format version using the provided key block authentication key (KBAK).\n\n"
	"NOTE:\nAll hex values are strings of hex digits representing binary data, or - to read raw bytes from stdin.",
};

// argp parser helper function
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state)
{
	int r;
	struct tr31_mac_options_t* options;
	void* buf = NULL;
	size_t buf_len = 0;

	options = state->input;
	if (!options) {
		return ARGP_ERR_UNKNOWN;
	}

	if (arg) {
		// Process hex/stdin arguments
		switch (key) {
			case TR31_MAC_OPTION_KBAK:
			case TR31_MAC_OPTION_DATA: {
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
					// Parse argument as hex data
					size_t arg_len = strlen(arg);
					if (arg_len % 2 != 0) {
						argp_error(state, "Hex string must have even number of digits");
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
						argp_error(state, "Hex string must consist of hex digits");
						free(buf);
						return EINVAL;
					}
				}

				break;
			}
		}
	}

	switch (key) {
		case TR31_MAC_OPTION_KBAK:
			if (buf_len > sizeof(options->kbak_buf)) {
				argp_error(state, "KEY string may not have more than %zu digits (thus %zu bytes)",
					sizeof(options->kbak_buf) * 2,
					sizeof(options->kbak_buf)
				);
				free(buf);
				return EINVAL;
			}
			memcpy(options->kbak_buf, buf, buf_len);
			options->kbak_buf_len = buf_len;
			options->kbak = true;

			free(buf);
			buf = NULL;

			return 0;

		case TR31_MAC_OPTION_VERSION_ID:
			if (strlen(arg) != 1) {
				argp_error(state, "Version must be a single character");
				return EINVAL;
			}
			options->version = *arg;
			options->have_version = true;
			return 0;

		case TR31_MAC_OPTION_DATA:
			options->data = buf;
			options->data_len = buf_len;
			options->have_data = true;
			return 0;

		case TR31_MAC_OPTION_VERIFY: {
			size_t arg_len = strlen(arg);
			if (arg_len % 2 != 0) {
				argp_error(state, "MAC string must have even number of digits");
				return EINVAL;
			}
			if (arg_len / 2 > sizeof(options->mac_verify)) {
				argp_error(state, "MAC string may not have more than %zu digits (thus %zu bytes)",
					sizeof(options->mac_verify) * 2,
					sizeof(options->mac_verify)
				);
				return EINVAL;
			}
			options->mac_verify_len = arg_len / 2;

			r = parse_hex(arg, options->mac_verify, options->mac_verify_len);
			if (r) {
				argp_error(state, "MAC string must consist of hex digits");
				return EINVAL;
			}
			options->verify = true;
			return 0;
		}

		case TR31_MAC_OPTION_VERSION: {
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
			if (!options->kbak) {
				argp_error(state, "The --kbak option is required");
				return EINVAL;
			}
			if (!options->have_version) {
				argp_error(state, "The --version-id option is required");
				return EINVAL;
			}
			if (!options->have_data) {
				argp_error(state, "The --data option is required");
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

// MAC generation helper function
static int do_tr31_mac_generate(const struct tr31_mac_options_t* options)
{
	int r;
	uint8_t mac[AES_CMAC_SIZE]; // large enough for all supported MAC algorithms
	size_t mac_len;

	switch (options->version) {
		case TR31_VERSION_A:
		case TR31_VERSION_C:
			mac_len = DES_CBCMAC_SIZE;
			r = crypto_tdes_cbcmac(options->kbak_buf, options->kbak_buf_len, options->data, options->data_len, mac);
			break;

		case TR31_VERSION_B:
			mac_len = DES_CMAC_SIZE;
			r = crypto_tdes_cmac(options->kbak_buf, options->kbak_buf_len, options->data, options->data_len, mac);
			break;

		case TR31_VERSION_D:
		case TR31_VERSION_E:
			mac_len = AES_CMAC_SIZE;
			r = crypto_aes_cmac(options->kbak_buf, options->kbak_buf_len, options->data, options->data_len, mac);
			break;

		default:
			fprintf(stderr, "%s\n", tr31_get_error_string(TR31_ERROR_UNSUPPORTED_VERSION));
			return 1;
	}
	if (r) {
		fprintf(stderr, "MAC generation failed; error %d: %s\n", r, tr31_get_error_string(r));
		return 1;
	}

	printf("MAC: ");
	print_hex(mac, mac_len);
	printf("\n");

	crypto_cleanse(mac, sizeof(mac));
	return 0;
}

// MAC verification helper function
static int do_tr31_mac_verify(const struct tr31_mac_options_t* options)
{
	int r;

	switch (options->version) {
		case TR31_VERSION_A:
		case TR31_VERSION_C:
			r = tr31_tdes_verify_cbcmac(
				options->kbak_buf,
				options->kbak_buf_len,
				options->data,
				options->data_len,
				options->mac_verify,
				options->mac_verify_len
			);
			break;

		case TR31_VERSION_B:
			r = tr31_tdes_verify_cmac(
				options->kbak_buf,
				options->kbak_buf_len,
				options->data,
				options->data_len,
				options->mac_verify,
				options->mac_verify_len
			);
			break;

		case TR31_VERSION_D:
		case TR31_VERSION_E:
			r = tr31_aes_verify_cmac(
				options->kbak_buf,
				options->kbak_buf_len,
				options->data,
				options->data_len,
				options->mac_verify,
				options->mac_verify_len
			);
			break;

		default:
			fprintf(stderr, "%s\n", tr31_get_error_string(TR31_ERROR_UNSUPPORTED_VERSION));
			return 1;
	}

	if (r) {
		printf("MAC verification failed\n");
		return 1;
	}

	printf("MAC verification successful\n");
	return 0;
}

int main(int argc, char** argv)
{
	int r;
	struct tr31_mac_options_t options;

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

	if (options.verify) {
		r = do_tr31_mac_verify(&options);
	} else {
		r = do_tr31_mac_generate(&options);
	}

exit:
	// Cleanup
	if (options.kbak) {
		crypto_cleanse(options.kbak_buf, sizeof(options.kbak_buf));
	}
	if (options.data) {
		free(options.data);
		options.data = NULL;
		options.data_len = 0;
	}

	return r;
}
