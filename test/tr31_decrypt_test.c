/**
 * @file tr31_decrypt_test.c
 *
 * Copyright 2020-2023, 2025-2026 Leon Lynch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "tr31.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// example data generated using a Thales payShield 10k HSM
static const uint8_t test1_kbpk[] = { 0xEF, 0xE0, 0x85, 0x3B, 0x25, 0x6B, 0x58, 0x3D, 0x86, 0x8F, 0x25, 0x1C, 0xE9, 0x9E, 0xA1, 0xD9 };
static const char test1_tr31_format_a[] = "A0072K0TN00N0000F40D5672C6D0EC86F860BA88D44D00F0CA9A8CE8CD2F640287A9A9EB";
static const char test1_tr31_format_b[] = "B0080K0TN00N00001C414014375212C24995E405B5EE052CB92B67F455EA2680F6751088F9F1C228";
static const char test1_tr31_format_c[] = "C0072K0TN00N0000C9B875FF7A5316BF221C09ED52080DE0B45632A4EA9CE87699CB565E";
static const uint8_t test1_tr31_key_verify[] = { 0x5D, 0xB5, 0x0B, 0x45, 0x4F, 0x83, 0x89, 0xAD, 0xCE, 0x57, 0x3B, 0xE5, 0x08, 0x61, 0xF2, 0xBF };
static const uint8_t test1_tr31_kcv_verify[] = { 0x5C, 0x94, 0x05 };

// TR-31:2018, A.7.2.1
static const uint8_t test2_kbpk[] = { 0x89, 0xE8, 0x8C, 0xF7, 0x93, 0x14, 0x44, 0xF3, 0x34, 0xBD, 0x75, 0x47, 0xFC, 0x3F, 0x38, 0x0C };
static const char test2_tr31_ascii[] = "A0072P0TE00E0000F5161ED902807AF26F1D62263644BD24192FDB3193C730301CEE8701";
static const uint8_t test2_tr31_key_verify[] = { 0xF0, 0x39, 0x12, 0x1B, 0xEC, 0x83, 0xD2, 0x6B, 0x16, 0x9B, 0xDC, 0xD5, 0xB2, 0x2A, 0xAF, 0x8F };
static const uint8_t test2_tr31_kcv_verify[] = { 0xCB, 0x9D, 0xEA };

// TR-31:2018, A.7.2.2
static const uint8_t test3_kbpk[] = { 0xDD, 0x75, 0x15, 0xF2, 0xBF, 0xC1, 0x7F, 0x85, 0xCE, 0x48, 0xF3, 0xCA, 0x25, 0xCB, 0x21, 0xF6 };
static const char test3_tr31_ascii[] = "B0080P0TE00E000094B420079CC80BA3461F86FE26EFC4A3B8E4FA4C5F5341176EED7B727B8A248E";
static const uint8_t test3_tr31_key_verify[] = { 0x3F, 0x41, 0x9E, 0x1C, 0xB7, 0x07, 0x94, 0x42, 0xAA, 0x37, 0x47, 0x4C, 0x2E, 0xFB, 0xF8, 0xB8 };
static const uint8_t test3_tr31_kcv_verify[] = { 0x57, 0xC4, 0x09 };

// TR-31:2018, A.7.3.1
static const uint8_t test4_kbpk[] = { 0xB8, 0xED, 0x59, 0xE0, 0xA2, 0x79, 0xA2, 0x95, 0xE9, 0xF5, 0xED, 0x79, 0x44, 0xFD, 0x06, 0xB9 };
static const char test4_tr31_ascii[] = "C0096B0TX12S0100KS1800604B120F9292800000BFB9B689CB567E66FC3FEE5AD5F52161FC6545B9D60989015D02155C";
static const uint8_t test4_tr31_ksn_verify[] = { 0x00, 0x60, 0x4B, 0x12, 0x0F, 0x92, 0x92, 0x80, 0x00, 0x00 };
static const uint8_t test4_tr31_key_verify[] = { 0xED, 0xB3, 0x80, 0xDD, 0x34, 0x0B, 0xC2, 0x62, 0x02, 0x47, 0xD4, 0x45, 0xF5, 0xB8, 0xD6, 0x78 };
static const uint8_t test4_tr31_kcv_verify[] = { 0xF4, 0xB0, 0x8D };

// TR-31:2018, A.7.3.2
static const uint8_t test5_kbpk[] = { 0x1D, 0x22, 0xBF, 0x32, 0x38, 0x7C, 0x60, 0x0A, 0xD9, 0x7F, 0x9B, 0x97, 0xA5, 0x13, 0x11, 0xAC };
static const char test5_tr31_ascii[] = "B0104B0TX12S0100KS1800604B120F9292800000BB68BE8680A400D9191AD4ECE45B6E6C0D21C4738A52190E248719E24B433627";
static const uint8_t test5_tr31_ksn_verify[] = { 0x00, 0x60, 0x4B, 0x12, 0x0F, 0x92, 0x92, 0x80, 0x00, 0x00 };
static const uint8_t test5_tr31_key_verify[] = { 0xE8, 0xBC, 0x63, 0xE5, 0x47, 0x94, 0x55, 0xE2, 0x65, 0x77, 0xF7, 0x15, 0xD5, 0x87, 0xFE, 0x68 };
static const uint8_t test5_tr31_kcv_verify[] = { 0x9A, 0x42, 0x12 };

// TR-31:2018, A.7.4
static const uint8_t test6_kbpk[] = {
	0x88, 0xE1, 0xAB, 0x2A, 0x2E, 0x3D, 0xD3, 0x8C, 0x1F, 0xA0, 0x39, 0xA5, 0x36, 0x50, 0x0C, 0xC8,
	0xA8, 0x7A, 0xB9, 0xD6, 0x2D, 0xC9, 0x2C, 0x01, 0x05, 0x8F, 0xA7, 0x9F, 0x44, 0x65, 0x7D, 0xE6,
};
static const char test6_tr31_ascii[] = "D0112P0AE00E0000B82679114F470F540165EDFBF7E250FCEA43F810D215F8D207E2E417C07156A27E8E31DA05F7425509593D03A457DC34";
static const uint8_t test6_tr31_key_verify[] = { 0x3F, 0x41, 0x9E, 0x1C, 0xB7, 0x07, 0x94, 0x42, 0xAA, 0x37, 0x47, 0x4C, 0x2E, 0xFB, 0xF8, 0xB8 };
static const uint8_t test6_tr31_kcv_verify[] = { 0x08, 0x79, 0x3E };

// example data generated using a Thales payShield 10k HSM
static const uint8_t test7_kbpk[] = {
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
};
static const char test7_tr31_ascii[] = "D0112B0TN00N000037DB9B046B7B0048785690759580ABC3B9842AB4BB7717B49E92528E575785D8123559376A2553B27BE94F054F4E971C";
static const uint8_t test7_tr31_key_verify[] = { 0x1F, 0xA1, 0xF7, 0xCE, 0xC7, 0x98, 0xD9, 0x15, 0x45, 0xDA, 0x8A, 0xE0, 0xC7, 0x79, 0x6B, 0xD9 };
static const uint8_t test7_tr31_kcv_verify[] = { 0xFF, 0x50, 0x87 };

// example data generated using a Thales payShield 10k HSM
static const uint8_t test8_kbpk[] = {
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
};
static const char test8_tr31_ascii[] = "D0144D0AN00N0000127862F945C2DED04530FAF7CDBC8B0BA10C7AA79BD5E0C2C5D6AC173BF588E4B19ACF1357178D50EA0AB193228E13958304FC6149632DFDCADF3A5B3D57E814";
static const uint8_t test8_tr31_key_verify[] = {
	0xBE, 0x19, 0xE6, 0xA0, 0x7A, 0x76, 0x0F, 0x10, 0xEF, 0x8E, 0x83, 0xA2, 0x26, 0xB6, 0x3A, 0xAD,
	0x14, 0x1F, 0x46, 0x3F, 0xDD, 0xD4, 0xF4, 0x7D, 0xB2, 0x44, 0xB4, 0x02, 0x3E, 0xC3, 0xCA, 0xCC,
};
static const uint8_t test8_tr31_kcv_verify[] = { 0x0A, 0x00, 0xE3 };

// ISO 20038:2017, B.2
static const uint8_t test9_kbpk[] = {
	0x32, 0x35, 0x36, 0x2D, 0x62, 0x69, 0x74, 0x20, 0x41, 0x45, 0x53, 0x20, 0x77, 0x72, 0x61, 0x70,
	0x70, 0x69, 0x6E, 0x67, 0x20, 0x28, 0x49, 0x53, 0x4F, 0x20, 0x32, 0x30, 0x30, 0x33, 0x38, 0x29,
};
static const char test9_tr31_ascii[] = "E0084B0TV16N0000B2AE5E26BBA7F246E84D5EA24167E208A6B66EF2E27E55A52DB52F0AEACB94C57547";
static const uint8_t test9_tr31_key_verify[] = {
	0x77, 0x72, 0x61, 0x70, 0x70, 0x65, 0x64, 0x20, 0x33, 0x44, 0x45, 0x53, 0x20, 0x6B, 0x65, 0x79,
};
static const uint8_t test9_tr31_kcv_verify[] = { 0xB2, 0x9D, 0x42 };

// ISO 20038:2017, B.3
static const uint8_t test10_kbpk[] = {
	0x32, 0x35, 0x36, 0x2D, 0x62, 0x69, 0x74, 0x20, 0x41, 0x45, 0x53, 0x20, 0x77, 0x72, 0x61, 0x70,
	0x70, 0x69, 0x6E, 0x67, 0x20, 0x28, 0x49, 0x53, 0x4F, 0x20, 0x32, 0x30, 0x30, 0x33, 0x38, 0x29,
};
static const char test10_tr31_ascii[] = "D0112M3TV16N000018462FA5903B8D2B82FEE26B29713C0BE7ED81601087F12252093D06FC0A012C1CF769AD0E3E9E4877166AB013FC22B4";
static const uint8_t test10_tr31_key_verify[] = {
	// ISO 20038:2017, B.3 provides this is the wrapped key, but it doesn't match the input data...
	//0x76, 0x72, 0x61, 0x70, 0x70, 0x65, 0x64, 0x20, 0x33, 0x44, 0x45, 0x53, 0x20, 0x6B, 0x65, 0x79,

	// ISO 20038:2017, B.3 MAC input data shows that this is the wrapped key...
	0x76, 0x73, 0x61, 0x70, 0x70, 0x64, 0x64, 0x20, 0x32, 0x45, 0x45, 0x52, 0x20, 0x6B, 0x64, 0x79,
};
static const uint8_t test10_tr31_kcv_verify[] = { 0xB2, 0x9D, 0x42 };

// ANSI X9.143:2021, 8.1
static const uint8_t test11_kbpk[] = {
	0x88, 0xE1, 0xAB, 0x2A, 0x2E, 0x3D, 0xD3, 0x8C, 0x1F, 0xA0, 0x39, 0xA5, 0x36, 0x50, 0x0C, 0xC8,
	0xA8, 0x7A, 0xB9, 0xD6, 0x2D, 0xC9, 0x2C, 0x01, 0x05, 0x8F, 0xA7, 0x9F, 0x44, 0x65, 0x7D, 0xE6,
};
static const char test11_tr31_ascii[] = "D0144P0AE00E00002C77FA3F4A553BED6E88AE5C172A4166E3D4ACA8E2AC71C158A476FAC12C13C3829DE55D3AB54C48F4C4FEF7AC75E90FC47F1B77E7B19A73ED46E64410082557";
static const uint8_t test11_tr31_key_verify[] = { 0x3F, 0x41, 0x9E, 0x1C, 0xB7, 0x07, 0x94, 0x42, 0xAA, 0x37, 0x47, 0x4C, 0x2E, 0xFB, 0xF8, 0xB8 };
static const uint8_t test11_tr31_kcv_verify[] = { 0x08, 0x79, 0x3E, 0x25, 0xAB };

// ANSI X9.143:2021, 8.2
// Unfortunately the key block provided by ANSI X9.143:2021 for this test is
// invalid because optional block KS contains invalid characters
/*
static const uint8_t test12_kbpk[] = {
	0xE3, 0x83, 0x31, 0xFB, 0xAC, 0xE3, 0x3F, 0x0B, 0x86, 0x94, 0xAB, 0xA5, 0xDC, 0x61, 0x1C, 0xA2,
	0x08, 0x31, 0x94, 0x9F, 0xEB, 0x89, 0x88, 0x10, 0x21, 0x47, 0x29, 0x15, 0x78, 0xF7, 0x04, 0xE1,
};
static const char test12_tr31_ascii[] = "D0192C0AVA1N0300KS08VM9ATS1A2018-06-18T20:42:39.22PB0E00000000005FDAFA00A1E84F599C2EB51A1F7A767D5E42314F0E84A3FC1A7B84C1DE81114659E6306AD544208F68F15602BD3E12DA0C7F9FC551F1C8E6385FAFC1F7B499F5";
static const char test12_tr31_ksn_verify[] = "VM9A";
static const char test12_tr31_ts_verify[] = "2018-06-18T20:42:39.22";
static const uint8_t test12_tr31_key_verify[] = {
	0x8C, 0x32, 0x60, 0x37, 0xF8, 0x91, 0x0B, 0xBF, 0xDB, 0xC2, 0x67, 0xE5, 0x10, 0x1D, 0xFB, 0xF9,
	0x48, 0x04, 0x33, 0x02, 0x8D, 0x5E, 0x67, 0xB3, 0x46, 0x73, 0x44, 0x0F, 0x8A, 0xCE, 0xC9, 0x72,
};
static const uint8_t test12_tr31_kcv_verify[] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
*/

// ANSI X9.143:2021, 8.3.2.1
// Unfortunately the key block provided by ANSI X9.143:2021 for this test is
// invalid because the header length is incorrect
/*
static const uint8_t test13_kbpk[] = { 0x89, 0xE8, 0x8C, 0xF7, 0x93, 0x14, 0x44, 0xF3, 0x34, 0xBD, 0x75, 0x47, 0xFC, 0x3F, 0x38, 0x0C };
static const char test13_tr31_ascii[] = "A0072P0TE00E0000A8974C06DBFD58D197101A28DEC1A6C7C23F00A3B18EC6D538DE4A5B5F49A542D61A8A8B";
static const uint8_t test13_tr31_key_verify[] = { 0xF0, 0x39, 0x12, 0x1B, 0xEC, 0x83, 0xD2, 0x6B, 0x16, 0x9B, 0xDC, 0xD5, 0xB2, 0x2A, 0xAF, 0x8F };
static const uint8_t test13_tr31_kcv_verify[] = { 0xCB, 0x9D, 0xEA };
*/

// ANSI X9.143:2021, 8.3.2.2
// Unfortunately the key block provided by ANSI X9.143:2021 for this test is
// invalid because the header length is incorrect
/*
static const uint8_t test14_kbpk[] = { 0xDD, 0x75, 0x15, 0xF2, 0xBF, 0xC1, 0x7F, 0x85, 0xCE, 0x48, 0xF3, 0xCA, 0x25, 0xCB, 0x21, 0xF6 };
static const char test14_tr31_ascii[] = "B0080P0TE00E000094B420079CC80BA3461F86FE26EFC4A38C6B0A146BF1B0BE0D3277F17A3AD5146EED7B727B8A248E";
static const uint8_t test14_tr31_key_verify[] = { 0x3F, 0x41, 0x9E, 0x1C, 0xB7, 0x07, 0x94, 0x42, 0xAA, 0x37, 0x47, 0x4C, 0x2E, 0xFB, 0xF8, 0xB8 };
static const uint8_t test14_tr31_kcv_verify[] = { 0x57, 0xC4, 0x09 };
*/

// ANSI X9.143:2021, 8.4.1
static const uint8_t test15_kbpk[] = { 0xB8, 0xED, 0x59, 0xE0, 0xA2, 0x79, 0xA2, 0x95, 0xE9, 0xF5, 0xED, 0x79, 0x44, 0xFD, 0x06, 0xB9 };
static const char test15_tr31_ascii[] = "C0112B0TX12S0100KS1800604B120F929280000042B758A2400AB598AE37782823DAF0BA4BDB0DAFF34915345CA169AE1F976A429EB139E5";
static const uint8_t test15_tr31_ksn_verify[] = { 0x00, 0x60, 0x4B, 0x12, 0x0F, 0x92, 0x92, 0x80, 0x00, 0x00 };
static const uint8_t test15_tr31_key_verify[] = { 0xED, 0xB3, 0x80, 0xDD, 0x34, 0x0B, 0xC2, 0x62, 0x02, 0x47, 0xD4, 0x45, 0xF5, 0xB8, 0xD6, 0x78 };
static const uint8_t test15_tr31_kcv_verify[] = { 0xF4, 0xB0, 0x8D };

// ANSI X9.143:2021, 8.4.2
static const uint8_t test16_kbpk[] = { 0x1D, 0x22, 0xBF, 0x32, 0x38, 0x7C, 0x60, 0x0A, 0xD9, 0x7F, 0x9B, 0x97, 0xA5, 0x13, 0x11, 0xAC };
static const char test16_tr31_ascii[] = "B0120B0TX12S0100KS1800604B120F929280000015CEB14B76D551F21EC43A75390FA118A98C6CB049E3B9E864A5F4A8B9A5108A6DB5635C95B042D7";
static const uint8_t test16_tr31_ksn_verify[] = { 0x00, 0x60, 0x4B, 0x12, 0x0F, 0x92, 0x92, 0x80, 0x00, 0x00 };
static const uint8_t test16_tr31_key_verify[] = { 0xE8, 0xBC, 0x63, 0xE5, 0x47, 0x94, 0x55, 0xE2, 0x65, 0x77, 0xF7, 0x15, 0xD5, 0x87, 0xFE, 0x68 };
static const uint8_t test16_tr31_kcv_verify[] = { 0x9A, 0x42, 0x12 };

// ANSI X9.143:2021, 8.5
static const uint8_t test17_kbpk[] = { 0xFA, 0x36, 0xE4, 0x42, 0x78, 0xDB, 0x3A, 0xB5, 0xF2, 0x98, 0xF9, 0xF7, 0xDA, 0x8F, 0x1F, 0x88 };
static const char test17_tr31_ascii[] =
	// Header
	"D3776S0RS00N0400CT0002050000MIIDszCCApugAwIBAgIIKpD5FKMfCZEwDQYJKoZIhvcNAQELBQAwLTEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxEjAQBgNVBAMMCVNhbXBsZSBDQTAeFw0yMDA4MTUwMjE0MTBaFw0yMTA4MTUwMjE0MTBaME8xFzAVBgNVBAoMDkFscGhhIE1lcmNoYW50MR8wHQYDVQQLDBZUTFMgQ2xpZW50IENlcnRpZmljYXRlMRMwEQYDVQQDDAoxMjM0NTY3ODkwMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA1sRg+wEuje3y14V0tFHpvxxpY/fyrldB0nRctBDn4AvkBfyJuDLG59vqkGXVd8J8YQdwEHZJrVq+7B8rjtM6PMoyH/7QAZZAC7tw740P4cfen1IryubZVviV9QUp+gHToelZfr1rfIsuEGhzo6UhwY70kkS87/rYHCVathZEjMmvUIEdpzg0PZ2+Heg6D35OQ70I+np+BsEQf71Zr+d2iKqVGEd50l8tbn4W3A4rOyUERPTaACwS9rvdF7nlmTqSI5ybN6lmm37a71h77n6M54aaw2KkJYWVo+1stUTyFVsv/YBs9aylbBHQOYqp/U2tB0TxM58QYGzyaWvNqbFzOQIDAQABo4G0MIGxMAkGA1UdEwQCMAAwDgYDVR0PAQH/BAQDAgeAMBMGA1UdJQQMMAoGCCsGAQUFBwMCMB0GA1UdDgQWBBR837QRAGx5uL9xDnRjr9L9WSBSlzAfBgNVHSMEGDAWgBSlXhVYy9bic9OLnRsxsFgKQQbLmTA/BgNVHR8EODA2MDSgMqAwhi5odHRwOi8vY3JsLmFscGhhLW1lcmNoYW50LmV4YW1wbGUvU2FtcGxlQ0EuY3JsMA0GCSqGSIb3DQEBCwUAA4IBAQCH6JusIBSkRDqzAohaSoJVAEwQGMdcUSQWDfMyJjZqkOep1kT8Sl7LolFmmmVRJdkTWZe4PxBfQUc/eIql9BIx90506B+j9aoVA7212OExAid78GgqKA6JoalhYQKRta9ixY8iolydTYyEYpegA1jFZavMQma4ZGwX/bDJWr4+cJYxJXWaf67g4AMqHaWC8J60MVjrrBe9BZ0ZstuIlNkktQUOZanqxqsrFeqz02ibwTwNHtaHQCztB4KgdTkrTNahkqeq6xjafDoTllNo1EddajnbA/cVzF9ZCNigDtg5chXHWIQbgEK7HmU3sY3/wd2Bh1KdF3+vpN+5iZMRNv7ZKP1001D77F007724TS1320200818221218ZPB0D000000000"

	// Encrypted data
	"6A5FE664385E390C880D12C1E81B74BD0C35D00A27085602753679E3F00FD00B953558F47F972F0A3CF03DA5F75EBAAE21DA27AFF0D1B8CD213F036635BAD24DE82DB1832C245BB38EE40BD530FF5BFFF3C35131AE1EAD3871E6569114BEBC8CE462AE992B5AC0F33C3E995FDB0225691CDF01BF695EC96F5442AB9650C22896E16C368C1FB796CCA72CA1CF2D403DB1D30A0D63E7E92D61A3A438C33C529E99C8036B6D2015F4693DD5C7949F6BC12AE6E33C2EED215DBB1E9CD6D8528B3F02B7108B568AF252C6F26C599BEEDB4F81A5FEF6E2F2D8888D327F15A8CED909C5D84061FC8F62A021CADA7C1D7B241EE5FB3FC359CED864523CB64E75BEC4262E5A31E93032DF64000D717D3AD78D0A62FE00243B76D1D3A39290EE9C980C9A38F33CF26B5FA7359EC396B8AD8D2945965110A6C35294797099D39827C62CB028E35E763FE69EE3C0D0E19A9B7520DA44B79ED18289C354E7BE339E567D5C80921033CF950955CAED0E0FB739A1DD793DAD7BCB3F0BA068AF8EEBFD771B0D8E86953F1B7A81150D7953BDFC70E760E187E3D4DAD57F179B3016B5032F12B6C227E3FDD3000D5198100AD95FC6B45EFFD0AD2772242D7EA6D73F186F391CA1777EDF9A78ACF4C62807DF4B8A43E91A519DC67E576D7B0ADC2D16C7FBFE6F67B05A484EC58DB7F8129FC7CAE61A158BB391A12EFCCF84D25791485B4A5CFDA75133EED967DEA9BDD1CC32A8D5A18ACE53A9C607EC8491C80335231B1CB77AAAAC3D113B21DD366CB2FF13A97961E13E48A5B1CA0A70026A7DD2F47529D7EC8055A0516043D6481DFC69A4779B13040C3E6FE8A4B73DD14306177BDC65DAFE6CD5BA6FD3A0071A7ACFF0BFED47784BEEF8E11D5398E704A6DC9CC99B22E8424B27D1C575373B0387FAA7140D344E3409235C16F478EC819CA11931EC6F4344D4C34A9B5E6F8617392A4E1DC625BEF83C767B135244FC75473F5CEAA44E2BE6C42A6AC0DA232184D0428DE8D62D888D62C28739931AF64A4493BEA1293A124C90AA9D00F19460C5CAD47FA37DBF1481835EE9B1A597E40C7023F825DE15BE57CD75BD6BEA55F12C0372B59D623C898B24957CDFA0FFF88751E765B9D63EABA200A9917B148A1E776FA362F7B5856381303315C02936C56CDDADE81840D05D4C900C5D3A5A76BA0A8447F2E7B86000AEC27037EBF254ADC0FABDBC113A09316308374940968193259C2DDFBE0B7FB7D090896CCC94E058BD835230E9BD3B70E5BC13510382F831DEF5D86CBD5885FED9F4B77C69EF0EFF87FE90EC619971C4CF4532FEE7A07EA8ABAEF7112CC8DE75448CA4AD145B8D763F8409CCB143D5F05B2D6CBFC64299883DE5F5E14AE9D53D46D50088402E8B36E2BDF276EA209657391EBD2AE28D1FFDE7C5F1DA5F455A830BFA59B60FB755ABF4964FA9E553D7D5565D8E69B1F42EA7E5C40896F29F14F8394C43A4CD62F89BDCDF28ED9DC293429EEBE63CEC0C8984DC650268AFC4F663FB457C35EA862F96076226D793B29095CE4FE2B91CA6543F3336EA1C7A8A1EDFD3FA375CEE82811FAB190483B2D2DDEB04C34E165B9444EE2AD52387ADBE9D92893ADC96B01B03C2AE026BDFB3DF90DBD8A3774C17BBBF8F1E6541EBDFE522D2EA9BD26EFC02C4674E6AB0B0321F8EBD3F5FAD035B0A7C4DDCAB24CE8A0F913FE1AA3B03"

	// MAC
	"654351B97C511BD2B6B6FA69E7A22CD5";
static const uint8_t test17_kcv_kbpk_verify[] = { 0xD7, 0x7F, 0x00, 0x77, 0x24 };
static const char test17_tr31_ts_verify[] = "20200818221218Z";
static const uint8_t test17_tr31_key_verify[] = {
	0x30, 0x82, 0x04, 0xA4, 0x02, 0x01, 0x00, 0x02, 0x82, 0x01, 0x01, 0x00, 0xD6, 0xC4, 0x60, 0xFB,
	0x01, 0x2E, 0x8D, 0xED, 0xF2, 0xD7, 0x85, 0x74, 0xB4, 0x51, 0xE9, 0xBF, 0x1C, 0x69, 0x63, 0xF7,
	0xF2, 0xAE, 0x57, 0x41, 0xD2, 0x74, 0x5C, 0xB4, 0x10, 0xE7, 0xE0, 0x0B, 0xE4, 0x05, 0xFC, 0x89,
	0xB8, 0x32, 0xC6, 0xE7, 0xDB, 0xEA, 0x90, 0x65, 0xD5, 0x77, 0xC2, 0x7C, 0x61, 0x07, 0x70, 0x10,
	0x76, 0x49, 0xAD, 0x5A, 0xBE, 0xEC, 0x1F, 0x2B, 0x8E, 0xD3, 0x3A, 0x3C, 0xCA, 0x32, 0x1F, 0xFE,
	0xD0, 0x01, 0x96, 0x40, 0x0B, 0xBB, 0x70, 0xEF, 0x8D, 0x0F, 0xE1, 0xC7, 0xDE, 0x9F, 0x52, 0x2B,
	0xCA, 0xE6, 0xD9, 0x56, 0xF8, 0x95, 0xF5, 0x05, 0x29, 0xFA, 0x01, 0xD3, 0xA1, 0xE9, 0x59, 0x7E,
	0xBD, 0x6B, 0x7C, 0x8B, 0x2E, 0x10, 0x68, 0x73, 0xA3, 0xA5, 0x21, 0xC1, 0x8E, 0xF4, 0x92, 0x44,
	0xBC, 0xEF, 0xFA, 0xD8, 0x1C, 0x25, 0x5A, 0xB6, 0x16, 0x44, 0x8C, 0xC9, 0xAF, 0x50, 0x81, 0x1D,
	0xA7, 0x38, 0x34, 0x3D, 0x9D, 0xBE, 0x1D, 0xE8, 0x3A, 0x0F, 0x7E, 0x4E, 0x43, 0xBD, 0x08, 0xFA,
	0x7A, 0x7E, 0x06, 0xC1, 0x10, 0x7F, 0xBD, 0x59, 0xAF, 0xE7, 0x76, 0x88, 0xAA, 0x95, 0x18, 0x47,
	0x79, 0xD2, 0x5F, 0x2D, 0x6E, 0x7E, 0x16, 0xDC, 0x0E, 0x2B, 0x3B, 0x25, 0x04, 0x44, 0xF4, 0xDA,
	0x00, 0x2C, 0x12, 0xF6, 0xBB, 0xDD, 0x17, 0xB9, 0xE5, 0x99, 0x3A, 0x92, 0x23, 0x9C, 0x9B, 0x37,
	0xA9, 0x66, 0x9B, 0x7E, 0xDA, 0xEF, 0x58, 0x7B, 0xEE, 0x7E, 0x8C, 0xE7, 0x86, 0x9A, 0xC3, 0x62,
	0xA4, 0x25, 0x85, 0x95, 0xA3, 0xED, 0x6C, 0xB5, 0x44, 0xF2, 0x15, 0x5B, 0x2F, 0xFD, 0x80, 0x6C,
	0xF5, 0xAC, 0xA5, 0x6C, 0x11, 0xD0, 0x39, 0x8A, 0xA9, 0xFD, 0x4D, 0xAD, 0x07, 0x44, 0xF1, 0x33,
	0x9F, 0x10, 0x60, 0x6C, 0xF2, 0x69, 0x6B, 0xCD, 0xA9, 0xB1, 0x73, 0x39, 0x02, 0x03, 0x01, 0x00,
	0x01, 0x02, 0x82, 0x01, 0x01, 0x00, 0x9A, 0x61, 0x93, 0xED, 0x1A, 0xCE, 0x62, 0x4B, 0xF7, 0xD2,
	0xA1, 0x26, 0x61, 0x30, 0xB8, 0xBC, 0x1E, 0x2A, 0x4C, 0x28, 0x42, 0x14, 0xBC, 0xB8, 0x9E, 0x15,
	0xF3, 0x45, 0xA5, 0x19, 0x69, 0x5E, 0x62, 0xCD, 0x42, 0xD9, 0xA4, 0xC5, 0x2B, 0x62, 0x24, 0x1D,
	0x9B, 0x2A, 0xF8, 0xA6, 0x1B, 0xF1, 0xD8, 0xB5, 0xC6, 0x02, 0xAF, 0x65, 0x0A, 0xEE, 0x3E, 0x6B,
	0xF1, 0x84, 0x18, 0x29, 0x12, 0xA5, 0xFC, 0x1A, 0xC8, 0x11, 0x1D, 0x68, 0xE6, 0x9E, 0xA7, 0x50,
	0x58, 0x40, 0x7A, 0xC0, 0x3D, 0xE6, 0xB4, 0xCB, 0x06, 0x00, 0x60, 0xDC, 0x4C, 0xC3, 0x4D, 0xF2,
	0x4D, 0xAD, 0x26, 0x9D, 0x86, 0x8E, 0xA0, 0xC6, 0xE3, 0x04, 0x4E, 0x19, 0x63, 0xEF, 0x90, 0x6F,
	0x4F, 0x06, 0x41, 0x4E, 0x44, 0xD3, 0xA4, 0x75, 0x7E, 0x67, 0x57, 0x01, 0x92, 0xE9, 0xA2, 0x61,
	0xDF, 0xB1, 0x20, 0x94, 0xAA, 0x36, 0x47, 0x65, 0x82, 0x27, 0x2E, 0xDA, 0xB0, 0xF5, 0x6F, 0x81,
	0x6D, 0x9F, 0xA6, 0x95, 0x80, 0xB3, 0xAB, 0x05, 0x32, 0x37, 0x13, 0x5B, 0xD1, 0xDD, 0xDB, 0x42,
	0xAF, 0x77, 0xE1, 0x1E, 0x16, 0x29, 0xF5, 0xA1, 0x1B, 0x22, 0xC5, 0xE3, 0xE2, 0xDB, 0x3E, 0x87,
	0x67, 0xA9, 0x0B, 0x94, 0x41, 0x48, 0x98, 0xDC, 0xCB, 0xB4, 0x7E, 0xFE, 0x61, 0x9F, 0x06, 0x20,
	0xAC, 0x29, 0xC3, 0x89, 0xFB, 0x46, 0x4C, 0xE9, 0xC5, 0xE2, 0x43, 0x96, 0x3E, 0x13, 0xB6, 0xDA,
	0x38, 0xEF, 0xAE, 0x13, 0x30, 0xAE, 0xFB, 0x54, 0xC0, 0xD5, 0xE2, 0xE5, 0x9B, 0x6D, 0x7F, 0xBE,
	0x4B, 0x3A, 0x22, 0xEE, 0x48, 0x3F, 0x74, 0xE7, 0x4D, 0x4A, 0x4A, 0x25, 0x97, 0x8C, 0x65, 0xF4,
	0xAC, 0x58, 0x29, 0xC3, 0x42, 0x60, 0x93, 0x0C, 0x85, 0xEC, 0xEA, 0x1F, 0xB2, 0x4D, 0xB5, 0x2A,
	0x43, 0x8D, 0x4E, 0xB2, 0xCF, 0x61, 0x02, 0x81, 0x81, 0x00, 0xED, 0x00, 0x3D, 0xEE, 0xB8, 0x01,
	0x3A, 0xF4, 0xE4, 0xEB, 0xE1, 0x72, 0xFE, 0x47, 0x4B, 0x23, 0xFE, 0x20, 0x12, 0x88, 0x05, 0x84,
	0x0C, 0x2D, 0x27, 0x7E, 0x3E, 0x30, 0x8D, 0x5B, 0x44, 0x52, 0x7F, 0x1E, 0xD3, 0x3C, 0x07, 0xAF,
	0x35, 0x0D, 0x5B, 0x22, 0xB2, 0x7E, 0x08, 0x2C, 0xD1, 0x01, 0xDD, 0x2D, 0xE5, 0x4D, 0xC8, 0xDF,
	0x8F, 0x91, 0xD4, 0xBA, 0x57, 0x68, 0xEB, 0x9E, 0xC5, 0xF2, 0x7D, 0xB5, 0xD3, 0x58, 0xEA, 0x5E,
	0x0D, 0xEE, 0x08, 0xA5, 0x35, 0x67, 0x7C, 0xB3, 0xF7, 0x65, 0x78, 0x9C, 0x9D, 0xAE, 0x56, 0xB7,
	0x42, 0x1B, 0x9E, 0x54, 0x52, 0x5E, 0xF9, 0x28, 0xAB, 0x28, 0x85, 0xBF, 0x09, 0x8E, 0x83, 0x79,
	0x99, 0xAD, 0x0C, 0xA3, 0xCA, 0x6A, 0xC6, 0x42, 0xEA, 0x9A, 0xD1, 0x33, 0x18, 0x56, 0xB0, 0xCC,
	0xEE, 0x5D, 0xF0, 0x1F, 0xED, 0x1B, 0x2F, 0x63, 0xC3, 0xA5, 0x02, 0x81, 0x81, 0x00, 0xE7, 0xFB,
	0xDA, 0x0A, 0x71, 0xC9, 0x26, 0xDD, 0x51, 0xF0, 0x37, 0x20, 0x6D, 0x90, 0x1A, 0x29, 0x75, 0x54,
	0xA3, 0x9B, 0xBC, 0x92, 0x39, 0x79, 0x44, 0x21, 0xF1, 0xF5, 0x4D, 0x29, 0x76, 0x6E, 0x50, 0xE0,
	0x16, 0xCA, 0x57, 0x01, 0xBD, 0xA7, 0x9A, 0xEC, 0x54, 0x3F, 0x50, 0x66, 0xB3, 0x73, 0x0E, 0x05,
	0x3E, 0xA4, 0xB5, 0x87, 0x2D, 0x25, 0xC2, 0x96, 0x73, 0xCA, 0x84, 0x57, 0xB0, 0x73, 0x90, 0xD1,
	0x1A, 0xF2, 0x3F, 0x22, 0x47, 0xA1, 0x13, 0x3F, 0xC2, 0x52, 0x2B, 0x96, 0xCB, 0x02, 0xDA, 0x77,
	0xD9, 0x27, 0xFE, 0xE6, 0x66, 0x10, 0x66, 0x05, 0x8D, 0x1A, 0x4D, 0x85, 0xFE, 0x3C, 0x1D, 0x34,
	0x18, 0x54, 0x2F, 0x24, 0xB3, 0x98, 0x2B, 0x4D, 0xDF, 0xB4, 0x19, 0x2E, 0x51, 0x2E, 0xDA, 0x1B,
	0xAA, 0xAA, 0x59, 0x95, 0x5B, 0x50, 0x94, 0x5D, 0xD0, 0x83, 0xE2, 0x6E, 0x4D, 0x05, 0x02, 0x81,
	0x80, 0x02, 0xD4, 0xE0, 0xE8, 0x8C, 0x3C, 0x3F, 0x87, 0x13, 0x81, 0x19, 0xF5, 0x74, 0xC2, 0x47,
	0x4C, 0x8B, 0xC9, 0xB8, 0x4E, 0xF5, 0xB9, 0xE9, 0x27, 0x54, 0xF4, 0x76, 0x2B, 0xC0, 0x54, 0x99,
	0xD1, 0x5E, 0x81, 0x70, 0xC6, 0xA3, 0xD4, 0xDD, 0x0E, 0x66, 0xCB, 0x58, 0x54, 0x97, 0x26, 0x69,
	0xEC, 0xDA, 0xC6, 0xA4, 0x99, 0xB4, 0x4F, 0xAF, 0x78, 0x6F, 0x91, 0x36, 0x60, 0x23, 0x88, 0x87,
	0x16, 0xE9, 0x97, 0x95, 0x89, 0xD7, 0x6A, 0xFE, 0x41, 0x9C, 0xCA, 0xD4, 0x83, 0x83, 0x02, 0xE7,
	0x6E, 0xC7, 0xED, 0x1F, 0x19, 0x29, 0x22, 0x11, 0x61, 0x21, 0x18, 0x22, 0xCF, 0xCD, 0xAC, 0x45,
	0xB7, 0x3B, 0x39, 0xD8, 0x14, 0x62, 0xCF, 0xBE, 0x1D, 0x4A, 0x2C, 0x5E, 0xCB, 0xBD, 0xC8, 0xA8,
	0xE2, 0xE6, 0xA2, 0xF4, 0xA4, 0x7C, 0x82, 0x46, 0x4A, 0xCB, 0x06, 0xA6, 0x9F, 0x8F, 0x86, 0x62,
	0x9D, 0x02, 0x81, 0x80, 0x4E, 0x98, 0xA9, 0x9A, 0xF8, 0x4A, 0x2A, 0x7C, 0xB9, 0x92, 0x25, 0x5B,
	0x3B, 0x43, 0xA3, 0x59, 0x80, 0x83, 0x18, 0x9B, 0x5F, 0x1C, 0x3B, 0x94, 0xB6, 0x5C, 0xB9, 0xD9,
	0x5E, 0x37, 0x3A, 0x04, 0xCE, 0x29, 0xDE, 0x0E, 0xD7, 0xC3, 0xA3, 0x39, 0xF1, 0xE7, 0x37, 0xF3,
	0xEB, 0x8D, 0xA0, 0x26, 0xCF, 0x0D, 0x3F, 0xD8, 0x16, 0x18, 0xA2, 0x57, 0x34, 0xC2, 0x3C, 0xA0,
	0xD4, 0x8D, 0xD1, 0x1E, 0x96, 0x66, 0x02, 0x37, 0x28, 0xE4, 0xB8, 0x57, 0xFE, 0x69, 0x8F, 0xB0,
	0xBF, 0x4B, 0xEB, 0xA4, 0x1F, 0xD8, 0x93, 0x1E, 0x55, 0xE2, 0x41, 0x9A, 0x34, 0xB6, 0x94, 0xC3,
	0xE0, 0x98, 0x11, 0x36, 0xD4, 0xBE, 0x1D, 0xB0, 0x07, 0xF8, 0xEB, 0x50, 0x16, 0xFB, 0xDF, 0x5A,
	0xE9, 0x5D, 0x23, 0xEC, 0x37, 0xC1, 0x3F, 0xE5, 0x4F, 0x4C, 0xA7, 0x0F, 0x79, 0xF4, 0xFE, 0xFC,
	0x6F, 0xEE, 0xE6, 0xF1, 0x02, 0x81, 0x81, 0x00, 0x83, 0x21, 0xC2, 0xF1, 0xE8, 0x91, 0x4C, 0x0A,
	0xE0, 0x9C, 0x41, 0x8A, 0xCF, 0xEB, 0xB8, 0xA7, 0xB8, 0x6A, 0x1E, 0x71, 0x44, 0x18, 0x2F, 0x51,
	0x45, 0xFB, 0xA9, 0x0A, 0xF1, 0x04, 0xDE, 0x3B, 0xC7, 0x60, 0x4D, 0x86, 0xA8, 0x31, 0xAC, 0x2F,
	0x38, 0xDA, 0x35, 0x6C, 0x99, 0xBC, 0x60, 0xBE, 0xA8, 0x0E, 0x26, 0xEC, 0x8B, 0x7F, 0xAF, 0x8B,
	0xB8, 0x4A, 0x86, 0x61, 0xEF, 0x56, 0x4B, 0xDC, 0x65, 0xDA, 0x05, 0x19, 0xF5, 0xE3, 0xCE, 0x81,
	0xFF, 0x49, 0x1C, 0xC7, 0x1D, 0xA0, 0x81, 0x39, 0x60, 0x04, 0x8B, 0x22, 0x5E, 0x61, 0xC5, 0x66,
	0x84, 0xD3, 0xCE, 0x01, 0xAE, 0x28, 0xA2, 0x12, 0xC9, 0xAC, 0xCE, 0x94, 0x6E, 0x2A, 0xAB, 0x80,
	0xAD, 0xD5, 0x1B, 0x00, 0x09, 0x30, 0x29, 0xC5, 0xD5, 0x2E, 0x9A, 0xF6, 0xC8, 0xA3, 0xEB, 0x86,
	0x16, 0x41, 0xB0, 0x0E, 0x23, 0x63, 0x6A, 0x68,
};

// ANSI X9.143:2021, 8.6
static const uint8_t test18_kbpk[] = {
	0x88, 0xE1, 0xAB, 0x2A, 0x2E, 0x3D, 0xD3, 0x8C, 0x1F, 0xA0, 0x39, 0xA5, 0x36, 0x50, 0x0C, 0xC8,
	0xA8, 0x7A, 0xB9, 0xD6, 0x2D, 0xC9, 0x2C, 0x01, 0x05, 0x8F, 0xA7, 0x9F, 0x44, 0x65, 0x7D, 0xE6,
};
static const char test18_tr31_ascii[] =
	// Header
	"D1840S0ES00N0400CT000205CC020002F0MIICLjCCAdSgAwIBAgIIGDrdWBxuNpAwCgYIKoZIzj0EAwIwMTEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxFjAUBgNVBAMMDVNhbXBsZSBFQ0MgQ0EwHhcNMjAwODE1MDIxMDEwWhcNMjEwODE1MDIxMDEwWjBPMRcwFQYDVQQKDA5BbHBoYSBNZXJjaGFudDEfMB0GA1UECwwWVExTIENsaWVudCBDZXJ0aWZpY2F0ZTETMBEGA1UEAwwKMTIzNDU2Nzg5MDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABEI/SLrH6fITA9y6Y3BneuoT/5+EHSepZxCYeSstGll2sVvmSDZWWSbN6lh5Fb/zagrDjjQ/gZtWIOTf2wL1vSGjgbcwgbQwCQYDVR0TBAIwADAOBgNVHQ8BAf8EBAMCB4AwEwYDVR0lBAwwCgYIKwYBBQUHAwIwHQYDVR0OBBYEFHuvP526vFMywEoVoXZ5aXNfhnfeMB8GA1UdIwQYMBaAFI+ZFhOWF+oMtcfYwg15vH5WmWccMEIGA1UdHwQ7MDkwN6A1oDOGMWh0dHA6Ly9jcmwuYWxwaGEtbWVyY2hhbnQuZXhhbXBsZS9TYW1wbGVFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSAAwRQIhAPuWWvCTmOdvQzUjCUmTX7H4sX4Ebpw+CI+aOQLu1DqwAiA0eR4FdMtvXV4P6+WMz5B10oea5xtLTfSgoBDoTkvKYQ==0002C4MIICDjCCAbOgAwIBAgIIfnOsCbsxHjwwCgYIKoZIzj0EAwIwNjEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxGzAZBgNVBAMMElNhbXBsZSBSb290IEVDQyBDQTAeFw0yMDA4MTUwMjEwMDlaFw0zMDA4MTMwMjEwMDlaMDExFzAVBgNVBAoMDkFscGhhIE1lcmNoYW50MRYwFAYDVQQDDA1TYW1wbGUgRUNDIENBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEHCanM9n+Rji+3EROj+HlogmXMU1Fk1td7N3I/8rfFnre1GwWCUqXSePHxwQ9DRHCV3oht3OUU2kDfitfUIujA6OBrzCBrDASBgNVHRMBAf8ECDAGAQH/AgEAMA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUj5kWE5YX6gy1x9jCDXm8flaZZxwwHwYDVR0jBBgwFoAUvElIifFlt6oeUaopV9Y0lJtyPVQwRgYDVR0fBD8wPTA7oDmgN4Y1aHR0cDovL2NybC5hbHBoYS1tZXJjaGFudC5leGFtcGxlL1NhbXBsZVJvb3RFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSQAwRgIhALT8+DG+++KuqqUGyBQ4YG4s34fqbujclxZTHxYWVVSNAiEAn3v5Xmct7fkLpkjGexiHsy6D90r0K2LlUqpN/069y5s=KP10012331550BC9TS1320200818004100ZPB110000000000000"

	// Encrypted data
	"8BDD0D63BA2E438AFD9386DDA768D6328C27A02C0009DD6E8EDB52D366DADE4FA1397E6DD7B360F115726BF4563D6DC303107FB63EC5B05F0837F6BD290E5A678AC15162B624CD2669092F9B8993FC492F1BA535EF9E1E55182A4DBE73DABC30945D96B329C04FCB325376D188112E16C8AD1AF5E3F2F234103C785B92FD24AD"

	// MAC
	"7E816121542513294E0A561F00BE2B53";
static const uint8_t test18_kcv_kbpk_verify[] = { 0x23, 0x31, 0x55, 0x0B, 0xC9 };
static const char test18_tr31_ts_verify[] = "20200818004100Z";
static const uint8_t test18_tr31_key_verify[] = {
	0x30, 0x77, 0x02, 0x01, 0x01, 0x04, 0x20, 0x2D, 0x49, 0x32, 0x57, 0xA4, 0x5B, 0x34, 0xC1, 0x1B,
	0x65, 0x26, 0xA0, 0x3D, 0xB4, 0xD8, 0xAE, 0x16, 0xEE, 0x87, 0xA0, 0xC1, 0x6B, 0xDF, 0x1B, 0xE2,
	0x3C, 0x2D, 0xD8, 0xB1, 0x64, 0xA2, 0xD3, 0xA0, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D,
	0x03, 0x01, 0x07, 0xA1, 0x44, 0x03, 0x42, 0x00, 0x04, 0x42, 0x3F, 0x48, 0xBA, 0xC7, 0xE9, 0xF2,
	0x13, 0x03, 0xDC, 0xBA, 0x63, 0x70, 0x67, 0x7A, 0xEA, 0x13, 0xFF, 0x9F, 0x84, 0x1D, 0x27, 0xA9,
	0x67, 0x10, 0x98, 0x79, 0x2B, 0x2D, 0x1A, 0x59, 0x76, 0xB1, 0x5B, 0xE6, 0x48, 0x36, 0x56, 0x59,
	0x26, 0xCD, 0xEA, 0x58, 0x79, 0x15, 0xBF, 0xF3, 0x6A, 0x0A, 0xC3, 0x8E, 0x34, 0x3F, 0x81, 0x9B,
	0x56, 0x20, 0xE4, 0xDF, 0xDB, 0x02, 0xF5, 0xBD, 0x21,
};

int main(void)
{
	int r;
	struct tr31_key_t test_kbpk;
	struct tr31_ctx_t test_tr31;
	const struct tr31_opt_ctx_t* opt_ctx;
	uint8_t ksn[20];
	struct tr31_opt_blk_kcv_data_t kcv_data;

	// populate key block protection key
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_TDES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test1_kbpk);
	test_kbpk.data = (void*)test1_kbpk;

	// test key block decryption for format version A
	printf("Test 1 (Basic format version A)...\n");
	r = tr31_import(test1_tr31_format_a, strlen(test1_tr31_format_a), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_A ||
		test_tr31.length != 72 ||
		test_tr31.key.usage != TR31_KEY_USAGE_KEK ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_TDES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_ANY ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_UNUSED ||
		test_tr31.key.key_version_str[0] != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_NONE ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test1_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 0 ||
		test_tr31.opt_blocks != NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test1_tr31_key_verify, sizeof(test1_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test1_tr31_kcv_verify, sizeof(test1_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// test key block decryption for format version B
	printf("Test 1 (Basic format version B)...\n");
	r = tr31_import(test1_tr31_format_b, strlen(test1_tr31_format_b), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_B ||
		test_tr31.length != 80 ||
		test_tr31.key.usage != TR31_KEY_USAGE_KEK ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_TDES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_ANY ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_UNUSED ||
		test_tr31.key.key_version_str[0] != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_NONE ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test1_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 0 ||
		test_tr31.opt_blocks != NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test1_tr31_key_verify, sizeof(test1_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test1_tr31_kcv_verify, sizeof(test1_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// test key block decryption for format version C
	printf("Test 1 (Basic format version C)...\n");
	r = tr31_import(test1_tr31_format_c, strlen(test1_tr31_format_c), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_C ||
		test_tr31.length != 72 ||
		test_tr31.key.usage != TR31_KEY_USAGE_KEK ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_TDES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_ANY ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_UNUSED ||
		test_tr31.key.key_version_str[0] != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_NONE ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test1_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 0 ||
		test_tr31.opt_blocks != NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test1_tr31_key_verify, sizeof(test1_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test1_tr31_kcv_verify, sizeof(test1_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// TR-31:2018, A.7.2.1
	printf("Test 2 (TR-31:2018, A.7.2.1)...\n");
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_TDES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test2_kbpk);
	test_kbpk.data = (void*)test2_kbpk;
	r = tr31_import(test2_tr31_ascii, strlen(test2_tr31_ascii), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_A ||
		test_tr31.length != 72 ||
		test_tr31.key.usage != TR31_KEY_USAGE_PEK ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_TDES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_ENC ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_UNUSED ||
		test_tr31.key.key_version_str[0] != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_TRUSTED ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test2_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 0 ||
		test_tr31.opt_blocks != NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test2_tr31_key_verify, sizeof(test2_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test2_tr31_kcv_verify, sizeof(test2_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// TR-31:2018, A.7.2.2
	printf("Test 3 (TR-31:2018, A.7.2.2)...\n");
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_TDES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test3_kbpk);
	test_kbpk.data = (void*)test3_kbpk;
	r = tr31_import(test3_tr31_ascii, strlen(test3_tr31_ascii), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_B ||
		test_tr31.length != 80 ||
		test_tr31.key.usage != TR31_KEY_USAGE_PEK ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_TDES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_ENC ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_UNUSED ||
		test_tr31.key.key_version_str[0] != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_TRUSTED ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test3_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 0 ||
		test_tr31.opt_blocks != NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test3_tr31_key_verify, sizeof(test3_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test3_tr31_kcv_verify, sizeof(test3_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// TR-31:2018, A.7.3.1
	printf("Test 4 (TR-31:2018, A.7.3.1)...\n");
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_TDES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test4_kbpk);
	test_kbpk.data = (void*)test4_kbpk;
	r = tr31_import(test4_tr31_ascii, strlen(test4_tr31_ascii), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_C ||
		test_tr31.length != 96 ||
		test_tr31.key.usage != TR31_KEY_USAGE_BDK ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_TDES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_DERIVE||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_VALID ||
		memcmp(test_tr31.key.key_version_str, "12", 2) != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_SENSITIVE ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test4_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 1 ||
		test_tr31.opt_blocks == NULL ||
		test_tr31.opt_blocks[0].id != TR31_OPT_BLOCK_KS ||
		test_tr31.opt_blocks[0].data == NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test4_tr31_key_verify, sizeof(test4_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test4_tr31_kcv_verify, sizeof(test4_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	opt_ctx = tr31_opt_block_find(&test_tr31, TR31_OPT_BLOCK_KS);
	if (opt_ctx != &test_tr31.opt_blocks[0]) {
		fprintf(stderr, "tr31_opt_block_find() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (opt_ctx->data_length != sizeof(test4_tr31_ksn_verify) * 2) {
		fprintf(stderr, "TR-31 optional block KS data length is incorrect\n");
		r = 1;
		goto exit;
	}
	memset(ksn, 0, sizeof(ksn));
	r = tr31_opt_block_decode_KS(opt_ctx, ksn, sizeof(test4_tr31_ksn_verify));
	if (r) {
		fprintf(stderr, "tr31_opt_block_decode_KS() failed; r=%d\n", r);
		goto exit;
	}
	if (memcmp(ksn, test4_tr31_ksn_verify, sizeof(test4_tr31_ksn_verify)) != 0) {
		fprintf(stderr, "TR-31 optional block KS decoded data is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// TR-31:2018, A.7.3.2
	printf("Test 5 (TR-31:2018, A.7.3.2)...\n");
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_TDES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test5_kbpk);
	test_kbpk.data = (void*)test5_kbpk;
	r = tr31_import(test5_tr31_ascii, strlen(test5_tr31_ascii), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_B ||
		test_tr31.length != 104 ||
		test_tr31.key.usage != TR31_KEY_USAGE_BDK ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_TDES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_DERIVE||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_VALID ||
		memcmp(test_tr31.key.key_version_str, "12", 2) != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_SENSITIVE ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test5_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 1 ||
		test_tr31.opt_blocks == NULL ||
		test_tr31.opt_blocks[0].id != TR31_OPT_BLOCK_KS ||
		test_tr31.opt_blocks[0].data == NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test5_tr31_key_verify, sizeof(test5_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test5_tr31_kcv_verify, sizeof(test5_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	opt_ctx = tr31_opt_block_find(&test_tr31, TR31_OPT_BLOCK_KS);
	if (opt_ctx != &test_tr31.opt_blocks[0]) {
		fprintf(stderr, "tr31_opt_block_find() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (opt_ctx->data_length != sizeof(test5_tr31_ksn_verify) * 2) {
		fprintf(stderr, "TR-31 optional block KS data length is incorrect\n");
		r = 1;
		goto exit;
	}
	memset(ksn, 0, sizeof(ksn));
	r = tr31_opt_block_decode_KS(opt_ctx, ksn, sizeof(test5_tr31_ksn_verify));
	if (r) {
		fprintf(stderr, "tr31_opt_block_decode_KS() failed; r=%d\n", r);
		goto exit;
	}
	if (memcmp(ksn, test5_tr31_ksn_verify, sizeof(test5_tr31_ksn_verify)) != 0) {
		fprintf(stderr, "TR-31 optional block KS decoded data is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// TR-31:2018, A.7.4
	printf("Test 6 (TR-31:2018, A.7.4)...\n");
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_AES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test6_kbpk);
	test_kbpk.data = (void*)test6_kbpk;
	r = tr31_import(test6_tr31_ascii, strlen(test6_tr31_ascii), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_D ||
		test_tr31.length != 112 ||
		test_tr31.key.usage != TR31_KEY_USAGE_PEK ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_AES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_ENC ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_UNUSED ||
		test_tr31.key.key_version_str[0] != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_TRUSTED ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test6_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 0 ||
		test_tr31.opt_blocks != NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test6_tr31_key_verify, sizeof(test6_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test6_tr31_kcv_verify, sizeof(test6_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// test key block decryption for format version D containing TDES key
	printf("Test 7 (Format version D containing TDES key)...\n");
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_AES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test7_kbpk);
	test_kbpk.data = (void*)test7_kbpk;
	r = tr31_import(test7_tr31_ascii, strlen(test7_tr31_ascii), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_D ||
		test_tr31.length != 112 ||
		test_tr31.key.usage != TR31_KEY_USAGE_BDK ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_TDES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_ANY ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_UNUSED ||
		test_tr31.key.key_version_str[0] != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_NONE ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test7_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 0 ||
		test_tr31.opt_blocks != NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test7_tr31_key_verify, sizeof(test7_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test7_tr31_kcv_verify, sizeof(test7_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// test key block decryption for format version D containing AES key
	printf("Test 8 (Format version D containing AES key)...\n");
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_AES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test8_kbpk);
	test_kbpk.data = (void*)test8_kbpk;
	r = tr31_import(test8_tr31_ascii, strlen(test8_tr31_ascii), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_D ||
		test_tr31.length != 144 ||
		test_tr31.key.usage != TR31_KEY_USAGE_DATA ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_AES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_ANY ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_UNUSED ||
		test_tr31.key.key_version_str[0] != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_NONE ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test8_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 0 ||
		test_tr31.opt_blocks != NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test8_tr31_key_verify, sizeof(test8_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test8_tr31_kcv_verify, sizeof(test8_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// ISO 20038:2017, B.2
	printf("Test 9 (ISO 20038:2017, B.2)...\n");
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_AES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test9_kbpk);
	test_kbpk.data = (void*)test9_kbpk;
	r = tr31_import(test9_tr31_ascii, strlen(test9_tr31_ascii), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_E ||
		test_tr31.length != 84 ||
		test_tr31.key.usage != TR31_KEY_USAGE_BDK ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_TDES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_MAC_VERIFY ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_VALID ||
		memcmp(test_tr31.key.key_version_str, "16", 2) != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_NONE ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test9_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 0 ||
		test_tr31.opt_blocks != NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test9_tr31_key_verify, sizeof(test9_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test9_tr31_kcv_verify, sizeof(test9_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// ISO 20038:2017, B.3
	printf("Test 10 (ISO 20038:2017, B.3)...\n");
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_AES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test10_kbpk);
	test_kbpk.data = (void*)test10_kbpk;
	r = tr31_import(test10_tr31_ascii, strlen(test10_tr31_ascii), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_D ||
		test_tr31.length != 112 ||
		test_tr31.key.usage != TR31_KEY_USAGE_ISO9797_1_MAC_3 ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_TDES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_MAC_VERIFY ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_VALID ||
		memcmp(test_tr31.key.key_version_str, "16", 2) != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_NONE ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test10_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 0 ||
		test_tr31.opt_blocks != NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test10_tr31_key_verify, sizeof(test10_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test10_tr31_kcv_verify, sizeof(test10_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// ANSI X9.143:2021, 8.1
	printf("Test 11 (ANSI X9.143:2021, 8.1)...\n");
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_AES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test11_kbpk);
	test_kbpk.data = (void*)test11_kbpk;
	r = tr31_import(test11_tr31_ascii, strlen(test11_tr31_ascii), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_D ||
		test_tr31.length != 144 ||
		test_tr31.key.usage != TR31_KEY_USAGE_PEK ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_AES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_ENC ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_UNUSED ||
		test_tr31.key.key_version_str[0] != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_TRUSTED ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test11_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 0 ||
		test_tr31.opt_blocks != NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test11_tr31_key_verify, sizeof(test11_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test11_tr31_kcv_verify, sizeof(test11_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// ANSI X9.143:2021, 8.4.1
	printf("Test 15 (ANSI X9.143:2021, 8.4.1)...\n");
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_TDES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test15_kbpk);
	test_kbpk.data = (void*)test15_kbpk;
	r = tr31_import(test15_tr31_ascii, strlen(test15_tr31_ascii), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_C ||
		test_tr31.length != 112 ||
		test_tr31.key.usage != TR31_KEY_USAGE_BDK ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_TDES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_DERIVE ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_VALID ||
		memcmp(test_tr31.key.key_version_str, "12", 2) != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_SENSITIVE ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test15_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 1 ||
		test_tr31.opt_blocks == NULL ||
		test_tr31.opt_blocks[0].id != TR31_OPT_BLOCK_KS ||
		test_tr31.opt_blocks[0].data == NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test15_tr31_key_verify, sizeof(test15_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test15_tr31_kcv_verify, sizeof(test15_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	opt_ctx = tr31_opt_block_find(&test_tr31, TR31_OPT_BLOCK_KS);
	if (opt_ctx != &test_tr31.opt_blocks[0]) {
		fprintf(stderr, "tr31_opt_block_find() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (opt_ctx->data_length != sizeof(test15_tr31_ksn_verify) * 2) {
		fprintf(stderr, "TR-31 optional block KS data length is incorrect\n");
		r = 1;
		goto exit;
	}
	memset(ksn, 0, sizeof(ksn));
	r = tr31_opt_block_decode_KS(opt_ctx, ksn, sizeof(test15_tr31_ksn_verify));
	if (r) {
		fprintf(stderr, "tr31_opt_block_decode_KS() failed; r=%d\n", r);
		goto exit;
	}
	if (memcmp(ksn, test15_tr31_ksn_verify, sizeof(test15_tr31_ksn_verify)) != 0) {
		fprintf(stderr, "TR-31 optional block KS decoded data is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// ANSI X9.143:2021, 8.4.2
	printf("Test 16 (ANSI X9.143:2021, 8.4.2)...\n");
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_TDES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test16_kbpk);
	test_kbpk.data = (void*)test16_kbpk;
	r = tr31_import(test16_tr31_ascii, strlen(test16_tr31_ascii), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_B ||
		test_tr31.length != 120 ||
		test_tr31.key.usage != TR31_KEY_USAGE_BDK ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_TDES ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_DERIVE ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_VALID ||
		memcmp(test_tr31.key.key_version_str, "12", 2) != 0 ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_SENSITIVE ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test16_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 1 ||
		test_tr31.opt_blocks == NULL ||
		test_tr31.opt_blocks[0].id != TR31_OPT_BLOCK_KS ||
		test_tr31.opt_blocks[0].data == NULL
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test16_tr31_key_verify, sizeof(test16_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.kcv, test16_tr31_kcv_verify, sizeof(test16_tr31_kcv_verify)) != 0) {
		fprintf(stderr, "TR-31 key data KCV is incorrect\n");
		r = 1;
		goto exit;
	}
	opt_ctx = tr31_opt_block_find(&test_tr31, TR31_OPT_BLOCK_KS);
	if (opt_ctx != &test_tr31.opt_blocks[0]) {
		fprintf(stderr, "tr31_opt_block_find() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (opt_ctx->data_length != sizeof(test16_tr31_ksn_verify) * 2) {
		fprintf(stderr, "TR-31 optional block KS data length is incorrect\n");
		r = 1;
		goto exit;
	}
	memset(ksn, 0, sizeof(ksn));
	r = tr31_opt_block_decode_KS(opt_ctx, ksn, sizeof(test16_tr31_ksn_verify));
	if (r) {
		fprintf(stderr, "tr31_opt_block_decode_KS() failed; r=%d\n", r);
		goto exit;
	}
	if (memcmp(ksn, test16_tr31_ksn_verify, sizeof(test16_tr31_ksn_verify)) != 0) {
		fprintf(stderr, "TR-31 optional block KS decoded data is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// ANSI X9.143:2021, 8.5
	printf("Test 17 (ANSI X9.143:2021, 8.5)...\n");
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_AES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test17_kbpk);
	test_kbpk.data = (void*)test17_kbpk;
	r = tr31_import(test17_tr31_ascii, strlen(test17_tr31_ascii), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_D ||
		test_tr31.length != 3776 ||
		test_tr31.key.usage != TR31_KEY_USAGE_AKP_SIG ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_RSA ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_SIG ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_UNUSED ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_NONE ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test17_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 4 ||
		test_tr31.opt_blocks == NULL ||
		test_tr31.opt_blocks[0].id != TR31_OPT_BLOCK_CT ||
		test_tr31.opt_blocks[0].data == NULL ||
		test_tr31.opt_blocks[1].id != TR31_OPT_BLOCK_KP ||
		test_tr31.opt_blocks[1].data == NULL ||
		test_tr31.opt_blocks[2].id != TR31_OPT_BLOCK_TS ||
		test_tr31.opt_blocks[2].data == NULL ||
		test_tr31.opt_blocks[3].id != TR31_OPT_BLOCK_PB
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test17_tr31_key_verify, sizeof(test17_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	opt_ctx = tr31_opt_block_find(&test_tr31, TR31_OPT_BLOCK_CT);
	if (opt_ctx != &test_tr31.opt_blocks[0]) {
		fprintf(stderr, "tr31_opt_block_find() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (opt_ctx->data_length != 0x500 - 10) { // total length - id and extended length field
		fprintf(stderr, "TR-31 optional block CT data length is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(opt_ctx->data, test17_tr31_ascii + 26, opt_ctx->data_length) != 0) {
		fprintf(stderr, "TR-31 optional block CT data is incorrect\n");
		r = 1;
		goto exit;
	}
	opt_ctx = tr31_opt_block_find(&test_tr31, TR31_OPT_BLOCK_KP);
	if (opt_ctx != &test_tr31.opt_blocks[1]) {
		fprintf(stderr, "tr31_opt_block_find() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (opt_ctx->data_length != (sizeof(test17_kcv_kbpk_verify) + 1) * 2) {
		fprintf(stderr, "TR-31 optional block KP data length is incorrect\n");
		r = 1;
		goto exit;
	}
	memset(&kcv_data, 0, sizeof(kcv_data));
	r = tr31_opt_block_decode_KP(opt_ctx, &kcv_data);
	if (r) {
		fprintf(stderr, "tr31_opt_block_decode_KP() failed; r=%d\n", r);
		goto exit;
	}
	if (kcv_data.kcv_algorithm != TR31_OPT_BLOCK_KCV_CMAC) {
		fprintf(stderr, "TR-31 optional block KP algorithm is incorrect\n");
		r = 1;
		goto exit;
	}
	if (kcv_data.kcv_len != sizeof(test17_kcv_kbpk_verify) ||
		memcmp(kcv_data.kcv, test17_kcv_kbpk_verify, sizeof(test17_kcv_kbpk_verify)) != 0
	) {
		fprintf(stderr, "TR-31 optional block KP data is incorrect\n");
		r = 1;
		goto exit;
	}
	opt_ctx = tr31_opt_block_find(&test_tr31, TR31_OPT_BLOCK_TS);
	if (opt_ctx != &test_tr31.opt_blocks[2]) {
		fprintf(stderr, "tr31_opt_block_find() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (opt_ctx->data_length != strlen(test17_tr31_ts_verify)) {
		fprintf(stderr, "TR-31 optional block TS data length is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(opt_ctx->data, test17_tr31_ts_verify, opt_ctx->data_length) != 0) {
		fprintf(stderr, "TR-31 optional block TS data is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	// ANSI X9.143:2021, 8.6
	printf("Test 18 (ANSI X9.143:2021, 8.6)...\n");
	memset(&test_kbpk, 0, sizeof(test_kbpk));
	test_kbpk.usage = TR31_KEY_USAGE_KEK;
	test_kbpk.algorithm = TR31_KEY_ALGORITHM_AES;
	test_kbpk.mode_of_use = TR31_KEY_MODE_OF_USE_ENC_DEC;
	test_kbpk.length = sizeof(test18_kbpk);
	test_kbpk.data = (void*)test18_kbpk;
	r = tr31_import(test18_tr31_ascii, strlen(test18_tr31_ascii), &test_kbpk, 0, &test_tr31);
	if (r) {
		fprintf(stderr, "tr31_import() error %d: %s\n", r, tr31_get_error_string(r));
		goto exit;
	}
	if (test_tr31.version != TR31_VERSION_D ||
		test_tr31.length != 1840 ||
		test_tr31.key.usage != TR31_KEY_USAGE_AKP_SIG ||
		test_tr31.key.algorithm != TR31_KEY_ALGORITHM_EC ||
		test_tr31.key.mode_of_use != TR31_KEY_MODE_OF_USE_SIG ||
		test_tr31.key.key_version != TR31_KEY_VERSION_IS_UNUSED ||
		test_tr31.key.exportability != TR31_KEY_EXPORT_NONE ||
		test_tr31.key.key_context != TR31_KEY_CONTEXT_NONE ||
		test_tr31.key.length != sizeof(test18_tr31_key_verify) ||
		test_tr31.key.data == NULL ||
		test_tr31.opt_blocks_count != 4 ||
		test_tr31.opt_blocks == NULL ||
		test_tr31.opt_blocks[0].id != TR31_OPT_BLOCK_CT ||
		test_tr31.opt_blocks[0].data == NULL ||
		test_tr31.opt_blocks[1].id != TR31_OPT_BLOCK_KP ||
		test_tr31.opt_blocks[1].data == NULL ||
		test_tr31.opt_blocks[2].id != TR31_OPT_BLOCK_TS ||
		test_tr31.opt_blocks[2].data == NULL ||
		test_tr31.opt_blocks[3].id != TR31_OPT_BLOCK_PB
	) {
		fprintf(stderr, "TR-31 context is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(test_tr31.key.data, test18_tr31_key_verify, sizeof(test18_tr31_key_verify)) != 0) {
		fprintf(stderr, "TR-31 key data is incorrect\n");
		r = 1;
		goto exit;
	}
	opt_ctx = tr31_opt_block_find(&test_tr31, TR31_OPT_BLOCK_CT);
	if (opt_ctx != &test_tr31.opt_blocks[0]) {
		fprintf(stderr, "tr31_opt_block_find() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (opt_ctx->data_length != 0x5CC - 10) { // total length - id and extended length field
		fprintf(stderr, "TR-31 optional block CT data length is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(opt_ctx->data, test18_tr31_ascii + 26, opt_ctx->data_length) != 0) {
		fprintf(stderr, "TR-31 optional block CT data is incorrect\n");
		r = 1;
		goto exit;
	}
	opt_ctx = tr31_opt_block_find(&test_tr31, TR31_OPT_BLOCK_KP);
	if (opt_ctx != &test_tr31.opt_blocks[1]) {
		fprintf(stderr, "tr31_opt_block_find() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (opt_ctx->data_length != (sizeof(test18_kcv_kbpk_verify) + 1) * 2) {
		fprintf(stderr, "TR-31 optional block KP data length is incorrect\n");
		r = 1;
		goto exit;
	}
	memset(&kcv_data, 0, sizeof(kcv_data));
	r = tr31_opt_block_decode_KP(opt_ctx, &kcv_data);
	if (r) {
		fprintf(stderr, "tr31_opt_block_decode_KP() failed; r=%d\n", r);
		goto exit;
	}
	if (kcv_data.kcv_algorithm != TR31_OPT_BLOCK_KCV_CMAC) {
		fprintf(stderr, "TR-31 optional block KP algorithm is incorrect\n");
		r = 1;
		goto exit;
	}
	if (kcv_data.kcv_len != sizeof(test18_kcv_kbpk_verify) ||
		memcmp(kcv_data.kcv, test18_kcv_kbpk_verify, sizeof(test18_kcv_kbpk_verify)) != 0
	) {
		fprintf(stderr, "TR-31 optional block KP data is incorrect\n");
		r = 1;
		goto exit;
	}
	opt_ctx = tr31_opt_block_find(&test_tr31, TR31_OPT_BLOCK_TS);
	if (opt_ctx != &test_tr31.opt_blocks[2]) {
		fprintf(stderr, "tr31_opt_block_find() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (opt_ctx->data_length != strlen(test18_tr31_ts_verify)) {
		fprintf(stderr, "TR-31 optional block TS data length is incorrect\n");
		r = 1;
		goto exit;
	}
	if (memcmp(opt_ctx->data, test18_tr31_ts_verify, opt_ctx->data_length) != 0) {
		fprintf(stderr, "TR-31 optional block TS data is incorrect\n");
		r = 1;
		goto exit;
	}
	tr31_release(&test_tr31);

	printf("All tests passed.\n");
	r = 0;
	goto exit;

exit:
	tr31_release(&test_tr31);
	return r;
}
