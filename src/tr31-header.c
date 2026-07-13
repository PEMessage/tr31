/**
 * @file tr31-header.c
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
#include "tr31_strings.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <stdlib.h>
#include <stdio.h>
#include <argp.h>

#include <ctype.h> // for isgraph

#ifdef _WIN32
// for _setmode
#include <fcntl.h>
#include <io.h>
#endif

// command line options
struct tr31_header_options_t {
	bool found_stdin_arg;
	bool header;

	// header parameters
	// valid if header is true
	size_t header_len;
	char* header_str;
	uint32_t import_flags;
};

// helper functions
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state);
static void* read_file(FILE* file, size_t* len);
static void print_hex(const void* buf, size_t length);
static void print_str(const void* buf, size_t length);
static void print_str_with_quotes(const void* buf, size_t length);

// argp option keys
enum tr31_header_option_keys_t {
	TR31_HEADER_OPTION_HEADER = -255, // negative value to avoid short options
	TR31_HEADER_OPTION_NO_STRICT_VALIDATION,
	TR31_HEADER_OPTION_VERSION,
};

// argp option structure
static struct argp_option argp_options[] = {
	{ "header", TR31_HEADER_OPTION_HEADER, "KEYBLOCK-HEADER", 0, "Key block header to decode. This may be a full key block; only the header is decoded. Use - to read raw bytes from stdin." },
	{ "no-strict-validation", TR31_HEADER_OPTION_NO_STRICT_VALIDATION, NULL, 0, "Disable strict validation during key block header decoding" },

	{ "version", TR31_HEADER_OPTION_VERSION, NULL, 0, "Display TR-31 library version" },

	{ 0 },
};

// argp configuration
static struct argp argp_config = {
	argp_options,
	argp_parser_helper,
	NULL,
	" \v" // force the text to be after the options in the help message
	"Decode the ASCII key block header into its individual attributes and "
	"optional blocks. No key block protection key is required.",
};

// argp parser helper function
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state)
{
	struct tr31_header_options_t* options;
	void* buf = NULL;
	size_t buf_len = 0;

	options = state->input;
	if (!options) {
		return ARGP_ERR_UNKNOWN;
	}

	if (arg) {
		// Process KEYBLOCK-HEADER argument
		switch (key) {
			case TR31_HEADER_OPTION_HEADER: {
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
					// Copy argument
					buf_len = strlen(arg);
					buf = malloc(buf_len);
					if (!buf) {
						argp_error(state, "Memory allocation failed");
						return ENOMEM;
					}
					memcpy(buf, arg, buf_len);
				}

				// Trim KEYBLOCK-HEADER argument
				for (char* str = buf; buf_len; --buf_len) {
					if (!isgraph(str[buf_len - 1])) {
						str[buf_len - 1] = 0;
					} else {
						break;
					}
				}

				break;
			}
		}
	}

	switch (key) {
		case TR31_HEADER_OPTION_HEADER:
			options->header_str = buf;
			options->header_len = buf_len;
			options->header = true;
			return 0;

		case TR31_HEADER_OPTION_NO_STRICT_VALIDATION:
			options->import_flags |= TR31_IMPORT_NO_STRICT_VALIDATION;
			return 0;

		case TR31_HEADER_OPTION_VERSION: {
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
			if (!options->header) {
				argp_error(state, "The --header option is required");
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

// hex output helper function
static void print_hex(const void* buf, size_t length)
{
	const uint8_t* ptr = buf;
	for (size_t i = 0; i < length; i++) {
		printf("%02X", ptr[i]);
	}
}

static void print_str(const void* buf, size_t length)
{
	char* str;

	if (!length) {
		return;
	}

	str = malloc(length + 1);
	if (!str) {
		return;
	}
	memcpy(str, buf, length);
	str[length] = 0;
	printf("%s", str);
	free(str);
}

static void print_str_with_quotes(const void* buf, size_t length)
{
	printf("\"");
	print_str(buf, length);
	printf("\"");
}

// key block header decode helper function
static int do_tr31_header(const struct tr31_header_options_t* options)
{
	int r;
	struct tr31_ctx_t tr31_ctx;
	char ascii_buf[3]; // temporary ascii buffer

	// decode key block header
	r = tr31_init_from_header(options->header_str, options->header_len, options->import_flags, &tr31_ctx);
	if (r) {
		fprintf(stderr, "TR-31 header decode error %d: %s\n", r, tr31_get_error_string(r));
		return r;
	}

	// print key block header details
	printf("Key block format version: %c\n", tr31_ctx.version);
	printf("Key usage: [%s] %s\n",
		tr31_key_usage_get_ascii(tr31_ctx.key.usage, ascii_buf, sizeof(ascii_buf)),
		tr31_key_usage_get_desc(&tr31_ctx)
	);
	printf("Key algorithm: [%c] %s\n",
		tr31_ctx.key.algorithm,
		tr31_key_algorithm_get_desc(&tr31_ctx)
	);
	printf("Key mode of use: [%c] %s\n",
		tr31_ctx.key.mode_of_use,
		tr31_key_mode_of_use_get_desc(&tr31_ctx)
	);
	switch (tr31_ctx.key.key_version) {
		case TR31_KEY_VERSION_IS_UNUSED: printf("Key version: Unused\n"); break;
		case TR31_KEY_VERSION_IS_VALID: printf("Key version: %s\n", tr31_ctx.key.key_version_str); break;
		case TR31_KEY_VERSION_IS_COMPONENT: printf("Key component: %c\n", tr31_ctx.key.key_version_str[1]); break;
	}
	printf("Key exportability: [%c] %s\n",
		tr31_ctx.key.exportability,
		tr31_key_exportability_get_desc(&tr31_ctx)
	);
	printf("Key context: [%c] %s\n",
		tr31_ctx.key.key_context,
		tr31_key_context_get_desc(&tr31_ctx)
	);

	// print optional blocks, if available
	if (tr31_ctx.opt_blocks_count) {
		printf("Optional blocks [%zu]:\n", tr31_ctx.opt_blocks_count);
	}
	if (tr31_ctx.opt_blocks) { // might be NULL when tr31_init_from_header() fails
		for (size_t i = 0; i < tr31_ctx.opt_blocks_count; ++i) {
			char opt_block_data_str[128];

			printf("\t[%s] %s: ",
				tr31_opt_block_id_get_ascii(tr31_ctx.opt_blocks[i].id, ascii_buf, sizeof(ascii_buf)),
				tr31_opt_block_id_get_desc(&tr31_ctx.opt_blocks[i])
			);

			switch (tr31_ctx.opt_blocks[i].id) {
				case TR31_OPT_BLOCK_AL: {
					struct tr31_opt_blk_akl_data_t akl_data;
					r = tr31_opt_block_decode_AL(&tr31_ctx.opt_blocks[i], &akl_data);
					if (r || akl_data.version != TR31_OPT_BLOCK_AL_VERSION_1) {
						// invalid; print as string
						print_str(tr31_ctx.opt_blocks[i].data, tr31_ctx.opt_blocks[i].data_length);
						break;
					}
					// valid; assume version 1 and print AKL as hex
					printf("v1, ");
					print_hex(&akl_data.v1.akl, sizeof(akl_data.v1.akl));
					break;
				}

				case TR31_OPT_BLOCK_BI: {
					struct tr31_opt_blk_bdkid_data_t bdkid_data;
					r = tr31_opt_block_decode_BI(&tr31_ctx.opt_blocks[i], &bdkid_data);
					if (r) {
						// invalid; print as string
						print_str(tr31_ctx.opt_blocks[i].data, tr31_ctx.opt_blocks[i].data_length);
						break;
					}
					// valid; print as hex
					print_hex(bdkid_data.bdkid, bdkid_data.bdkid_len);
					break;
				}

				case TR31_OPT_BLOCK_DA: {
					size_t da_attr_count;
					size_t da_data_len;
					struct tr31_opt_blk_da_data_t* da_data;
					if (tr31_ctx.opt_blocks[i].data_length < 2) {
						// invalid; print as string
						print_str(tr31_ctx.opt_blocks[i].data, tr31_ctx.opt_blocks[i].data_length);
						break;
					}
					da_attr_count = (tr31_ctx.opt_blocks[i].data_length - 2) / 5;
					da_data_len = sizeof(struct tr31_opt_blk_da_attr_t)
						* da_attr_count
						+ sizeof(struct tr31_opt_blk_da_data_t);
					da_data = malloc(da_data_len);
					if (!da_data) {
						// fallback; print as string
						print_str(tr31_ctx.opt_blocks[i].data, tr31_ctx.opt_blocks[i].data_length);
						break;
					}
					r = tr31_opt_block_decode_DA(&tr31_ctx.opt_blocks[i], da_data, da_data_len);
					if (r) {
						// invalid; print as string
						print_str(tr31_ctx.opt_blocks[i].data, tr31_ctx.opt_blocks[i].data_length);
						free(da_data);
						break;
					}
					for (size_t j = 0; j < da_attr_count; ++j) {
						printf("%s%s%c%c%c",
							j == 0 ? "" : ",",
							tr31_key_usage_get_ascii(da_data->attr[j].key_usage, ascii_buf, sizeof(ascii_buf)),
							da_data->attr[j].algorithm,
							da_data->attr[j].mode_of_use,
							da_data->attr[j].exportability
						);
					}
					free(da_data);
					break;
				}

				case TR31_OPT_BLOCK_HM: {
					uint8_t hash_algorithm;
					r = tr31_opt_block_decode_HM(&tr31_ctx.opt_blocks[i], &hash_algorithm);
					if (r) {
						// invalid; print as string
						print_str(tr31_ctx.opt_blocks[i].data, tr31_ctx.opt_blocks[i].data_length);
						break;
					}
					// valid; print as hex
					print_hex(&hash_algorithm, sizeof(hash_algorithm));
					break;
				}

				case TR31_OPT_BLOCK_IK: {
					uint8_t ikid[8];
					r = tr31_opt_block_decode_IK(&tr31_ctx.opt_blocks[i], ikid, sizeof(ikid));
					if (r) {
						// invalid; print as string
						print_str(tr31_ctx.opt_blocks[i].data, tr31_ctx.opt_blocks[i].data_length);
						break;
					}
					// valid; print as hex
					print_hex(ikid, sizeof(ikid));
					break;
				}

				case TR31_OPT_BLOCK_KS: {
					uint8_t iksn[10];
					r = tr31_opt_block_decode_KS(&tr31_ctx.opt_blocks[i], iksn, sizeof(iksn));
					if (r) {
						// invalid; print as string
						print_str(tr31_ctx.opt_blocks[i].data, tr31_ctx.opt_blocks[i].data_length);
						break;
					}
					// valid; print as hex
					print_hex(iksn, sizeof(iksn));
					break;
				}

				case TR31_OPT_BLOCK_KC:
				case TR31_OPT_BLOCK_KP:
				case TR31_OPT_BLOCK_PK: {
					struct tr31_opt_blk_kcv_data_t kcv_data;
					r = tr31_opt_block_decode_kcv(&tr31_ctx.opt_blocks[i], &kcv_data);
					if (r) {
						// invalid; print as string
						print_str(tr31_ctx.opt_blocks[i].data, tr31_ctx.opt_blocks[i].data_length);
						break;
					}
					// valid; print as hex
					print_hex(kcv_data.kcv, kcv_data.kcv_len);
					break;
				}

				case TR31_OPT_BLOCK_WP: {
					struct tr31_opt_blk_wp_data_t wp_data;
					r = tr31_opt_block_decode_WP(&tr31_ctx.opt_blocks[i], &wp_data);
					if (r || wp_data.version != TR31_OPT_BLOCK_WP_VERSION_0) {
						// invalid; print as string
						print_str(tr31_ctx.opt_blocks[i].data, tr31_ctx.opt_blocks[i].data_length);
						break;
					}
					// valid; assume version 00 and print wrapping pedigree digit
					print_str(tr31_ctx.opt_blocks[i].data + 2, 1);
					break;
				}

				case TR31_OPT_BLOCK_CT:
					// for certificates and certificate chains, skip the first two bytes and use quotes
					// the first byte will be decoded by tr31_opt_block_data_get_desc()
					print_str_with_quotes(tr31_ctx.opt_blocks[i].data + 2, tr31_ctx.opt_blocks[i].data_length - 2);
					break;

				case TR31_OPT_BLOCK_KV:
				case TR31_OPT_BLOCK_LB:
				case TR31_OPT_BLOCK_PB:
				case TR31_OPT_BLOCK_TC:
				case TR31_OPT_BLOCK_TS:
					print_str_with_quotes(tr31_ctx.opt_blocks[i].data, tr31_ctx.opt_blocks[i].data_length);
					break;

				// print all other optional blocks, including proprietary ones, verbatim
				default:
					print_str(tr31_ctx.opt_blocks[i].data, tr31_ctx.opt_blocks[i].data_length);
			}

			r = tr31_opt_block_data_get_desc(&tr31_ctx.opt_blocks[i], opt_block_data_str, sizeof(opt_block_data_str));
			if (r == TR31_ERROR_INVALID_OPTIONAL_BLOCK_DATA) {
				printf(" (Invalid)");
			} else if (r == 0 && opt_block_data_str[0]) {
				printf(" (%s)", opt_block_data_str);
			}

			printf("\n");
		}
	}

	// cleanup
	tr31_release(&tr31_ctx);

	return 0;
}

int main(int argc, char** argv)
{
	int r;
	struct tr31_header_options_t options;

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

	r = do_tr31_header(&options);

exit:
	// Cleanup
	if (options.header_str) {
		free(options.header_str);
		options.header_str = NULL;
		options.header_len = 0;
	}

	return r;
}
