/**
 * @file tr31-crypt.c
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

// selected cipher
enum tr31_crypt_cipher_t {
	TR31_CRYPT_CIPHER_NONE = 0,
	TR31_CRYPT_CIPHER_TDES_CBC,
	TR31_CRYPT_CIPHER_AES_CBC,
	TR31_CRYPT_CIPHER_AES_CTR,
};

// command line options
struct tr31_crypt_options_t {
	bool found_stdin_arg;
	bool kbek;
	bool have_iv;
	bool have_data;
	bool decrypt;

	// key block encryption key parameters
	// valid if kbek is true
	size_t kbek_buf_len;
	uint8_t kbek_buf[32]; // max 256-bit KBEK

	// selected cipher
	enum tr31_crypt_cipher_t cipher;

	// initialization vector / nonce
	// valid if have_iv is true
	uint8_t iv_buf[16]; // max AES block size
	size_t iv_buf_len;

	// input data to encrypt/decrypt
	// valid if have_data is true
	void* data;
	size_t data_len;
};

// helper functions
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state);
static void* read_file(FILE* file, size_t* len);
static int parse_hex(const char* hex, void* bin, size_t bin_len);
static void print_hex(const void* buf, size_t length);

// argp option keys
enum tr31_crypt_option_keys_t {
	TR31_CRYPT_OPTION_KBEK = -255, // negative value to avoid short options
	TR31_CRYPT_OPTION_CIPHER,
	TR31_CRYPT_OPTION_VERSION_ID,
	TR31_CRYPT_OPTION_IV,
	TR31_CRYPT_OPTION_DATA,
	TR31_CRYPT_OPTION_DECRYPT,
	TR31_CRYPT_OPTION_ENCRYPT,
	TR31_CRYPT_OPTION_VERSION,
};

// argp option structure
static struct argp_option argp_options[] = {
	{ "kbek", TR31_CRYPT_OPTION_KBEK, "KEY", 0, "Key block encryption key (KBEK). Use - to read raw bytes from stdin. See tr31-kbpk." },
	{ "cipher", TR31_CRYPT_OPTION_CIPHER, "tdes-cbc|aes-cbc|aes-ctr", 0, "Cipher to use. Alternatively use --version-id." },
	{ "version-id", TR31_CRYPT_OPTION_VERSION_ID, "A|B|C|D|E", 0, "Key block format version, selecting the cipher: A/B/C use tdes-cbc, D uses aes-cbc, E uses aes-ctr." },
	{ "iv", TR31_CRYPT_OPTION_IV, "IV", 0, "Initialization vector / nonce. For key block payloads this is the header (versions A/C) or the authenticator (versions B/D/E)." },
	{ "data", TR31_CRYPT_OPTION_DATA, "DATA", 0, "Data to encrypt or decrypt. Use - to read raw bytes from stdin." },
	{ "decrypt", TR31_CRYPT_OPTION_DECRYPT, NULL, 0, "Decrypt the data (default is to encrypt)." },
	{ "encrypt", TR31_CRYPT_OPTION_ENCRYPT, NULL, 0, "Encrypt the data (default)." },

	{ "version", TR31_CRYPT_OPTION_VERSION, NULL, 0, "Display TR-31 library version" },

	{ 0 },
};

// argp configuration
static struct argp argp_config = {
	argp_options,
	argp_parser_helper,
	NULL,
	" \v" // force the text to be after the options in the help message
	"Encrypt or decrypt a key block payload using the provided key block "
	"encryption key (KBEK), cipher and initialization vector.\n\n"
	"NOTE:\nAll hex values are strings of hex digits representing binary data, or - to read raw bytes from stdin.",
};

// argp parser helper function
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state)
{
	int r;
	struct tr31_crypt_options_t* options;
	void* buf = NULL;
	size_t buf_len = 0;

	options = state->input;
	if (!options) {
		return ARGP_ERR_UNKNOWN;
	}

	if (arg) {
		// Process hex/stdin arguments
		switch (key) {
			case TR31_CRYPT_OPTION_KBEK:
			case TR31_CRYPT_OPTION_DATA: {
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
		case TR31_CRYPT_OPTION_KBEK:
			if (buf_len > sizeof(options->kbek_buf)) {
				argp_error(state, "KEY string may not have more than %zu digits (thus %zu bytes)",
					sizeof(options->kbek_buf) * 2,
					sizeof(options->kbek_buf)
				);
				free(buf);
				return EINVAL;
			}
			memcpy(options->kbek_buf, buf, buf_len);
			options->kbek_buf_len = buf_len;
			options->kbek = true;

			free(buf);
			buf = NULL;

			return 0;

		case TR31_CRYPT_OPTION_CIPHER:
			if (strcmp(arg, "tdes-cbc") == 0) {
				options->cipher = TR31_CRYPT_CIPHER_TDES_CBC;
			} else if (strcmp(arg, "aes-cbc") == 0) {
				options->cipher = TR31_CRYPT_CIPHER_AES_CBC;
			} else if (strcmp(arg, "aes-ctr") == 0) {
				options->cipher = TR31_CRYPT_CIPHER_AES_CTR;
			} else {
				argp_error(state, "Cipher must be one of tdes-cbc, aes-cbc or aes-ctr");
				return EINVAL;
			}
			return 0;

		case TR31_CRYPT_OPTION_VERSION_ID:
			if (strlen(arg) != 1) {
				argp_error(state, "Version must be a single character");
				return EINVAL;
			}
			switch (*arg) {
				case TR31_VERSION_A:
				case TR31_VERSION_B:
				case TR31_VERSION_C:
					options->cipher = TR31_CRYPT_CIPHER_TDES_CBC;
					break;

				case TR31_VERSION_D:
					options->cipher = TR31_CRYPT_CIPHER_AES_CBC;
					break;

				case TR31_VERSION_E:
					options->cipher = TR31_CRYPT_CIPHER_AES_CTR;
					break;

				default:
					argp_error(state, "%s", tr31_get_error_string(TR31_ERROR_UNSUPPORTED_VERSION));
					return EINVAL;
			}
			return 0;

		case TR31_CRYPT_OPTION_IV: {
			size_t arg_len = strlen(arg);
			if (arg_len % 2 != 0) {
				argp_error(state, "IV string must have even number of digits");
				return EINVAL;
			}
			if (arg_len / 2 > sizeof(options->iv_buf)) {
				argp_error(state, "IV string may not have more than %zu digits (thus %zu bytes)",
					sizeof(options->iv_buf) * 2,
					sizeof(options->iv_buf)
				);
				return EINVAL;
			}
			options->iv_buf_len = arg_len / 2;

			r = parse_hex(arg, options->iv_buf, options->iv_buf_len);
			if (r) {
				argp_error(state, "IV string must consist of hex digits");
				return EINVAL;
			}
			options->have_iv = true;
			return 0;
		}

		case TR31_CRYPT_OPTION_DATA:
			options->data = buf;
			options->data_len = buf_len;
			options->have_data = true;
			return 0;

		case TR31_CRYPT_OPTION_DECRYPT:
			options->decrypt = true;
			return 0;

		case TR31_CRYPT_OPTION_ENCRYPT:
			options->decrypt = false;
			return 0;

		case TR31_CRYPT_OPTION_VERSION: {
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
			if (!options->kbek) {
				argp_error(state, "The --kbek option is required");
				return EINVAL;
			}
			if (options->cipher == TR31_CRYPT_CIPHER_NONE) {
				argp_error(state, "Either the --cipher option or the --version-id option is required");
				return EINVAL;
			}
			if (!options->have_iv) {
				argp_error(state, "The --iv option is required");
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

// payload encryption/decryption helper function
static int do_tr31_crypt(const struct tr31_crypt_options_t* options)
{
	int r;
	uint8_t* output = NULL;

	output = malloc(options->data_len);
	if (!output) {
		fprintf(stderr, "Memory allocation failed\n");
		return 1;
	}

	switch (options->cipher) {
		case TR31_CRYPT_CIPHER_TDES_CBC:
			if (options->decrypt) {
				r = crypto_tdes_decrypt(options->kbek_buf, options->kbek_buf_len, options->iv_buf, options->data, options->data_len, output);
			} else {
				r = crypto_tdes_encrypt(options->kbek_buf, options->kbek_buf_len, options->iv_buf, options->data, options->data_len, output);
			}
			break;

		case TR31_CRYPT_CIPHER_AES_CBC:
			if (options->decrypt) {
				r = crypto_aes_decrypt(options->kbek_buf, options->kbek_buf_len, options->iv_buf, options->data, options->data_len, output);
			} else {
				r = crypto_aes_encrypt(options->kbek_buf, options->kbek_buf_len, options->iv_buf, options->data, options->data_len, output);
			}
			break;

		case TR31_CRYPT_CIPHER_AES_CTR:
			if (options->decrypt) {
				r = crypto_aes_decrypt_ctr(options->kbek_buf, options->kbek_buf_len, options->iv_buf, options->data, options->data_len, output);
			} else {
				r = crypto_aes_encrypt_ctr(options->kbek_buf, options->kbek_buf_len, options->iv_buf, options->data, options->data_len, output);
			}
			break;

		default:
			fprintf(stderr, "%s\n", tr31_get_error_string(-1));
			free(output);
			return 1;
	}
	if (r) {
		fprintf(stderr, "%scryption failed; error %d: %s\n",
			options->decrypt ? "De" : "En",
			r,
			tr31_get_error_string(r)
		);
		crypto_cleanse(output, options->data_len);
		free(output);
		return 1;
	}

	print_hex(output, options->data_len);
	printf("\n");

	crypto_cleanse(output, options->data_len);
	free(output);
	return 0;
}

int main(int argc, char** argv)
{
	int r;
	struct tr31_crypt_options_t options;

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

	r = do_tr31_crypt(&options);

exit:
	// Cleanup
	if (options.kbek) {
		crypto_cleanse(options.kbek_buf, sizeof(options.kbek_buf));
	}
	if (options.data) {
		crypto_cleanse(options.data, options.data_len);
		free(options.data);
		options.data = NULL;
		options.data_len = 0;
	}

	return r;
}
