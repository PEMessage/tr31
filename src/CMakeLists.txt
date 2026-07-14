##############################################################################
# Copyright 2020-2023, 2025-2026 Leon Lynch
#
# This file is licensed under the terms of the LGPL v2.1 license.
# See LICENSE file.
##############################################################################

cmake_minimum_required(VERSION 3.22)

# assume that only GCC supports __attribute__((nonstring))
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
	set(HAVE_ATTRIBUTE_NONSTRING TRUE)
	set(NONSTRING "__attribute__((nonstring))")
else()
	set(HAVE_ATTRIBUTE_NONSTRING FALSE)
	unset(NONSTRING)
endif()

# check for headers that provide ntohs/htons and friends
include(CheckIncludeFile)
CHECK_INCLUDE_FILE(
	arpa/inet.h
	HAVE_ARPA_INET_H
)
CHECK_INCLUDE_FILE(
	winsock.h
	HAVE_WINSOCK_H
)
if(NOT HAVE_ARPA_INET_H AND NOT HAVE_WINSOCK_H)
	message(FATAL_ERROR "Failed to find either arpa/inet.h or winsock.h")
endif()

# build TR-31 tool by default
option(BUILD_TR31_TOOL "Build tr31-tool" ON)

# check for argp or allow the FETCH_ARGP option to download and build a local
# copy of libargp for monolithic builds on platforms without package managers
# like MacOS and Windows.
option(FETCH_ARGP "Download and build libargp")
if(FETCH_ARGP)
	set(ARGP_OSX_ARCHITECTURES ${CMAKE_OSX_ARCHITECTURES})
	include(FetchLibArgp)
else()
	find_package(argp)
endif()
if(NOT argp_FOUND)
	if(BUILD_TR31_TOOL)
		message(FATAL_ERROR "Could NOT find argp. Enable FETCH_ARGP to download and build libargp. This is required to build tr31-tool.")
	endif()
endif()

# check for features required for date/time conversion:
# locale.h, time.h, setlocale(), strptime(), timegm(), _mkgmtime(),
# localtime_r(), and gmtime_r()
include(CheckIncludeFile)
include(CheckSymbolExists)
CHECK_INCLUDE_FILE(locale.h HAVE_LOCALE_H)
CHECK_INCLUDE_FILE(time.h HAVE_TIME_H)
check_symbol_exists(setlocale locale.h HAVE_SETLOCALE)
check_symbol_exists(strptime time.h HAVE_STRPTIME)
set(TIME_H_DEFINITIONS_TRY _GNU_SOURCE) # Retry using _GNU_SOURCE
if(NOT HAVE_STRPTIME)
	unset(HAVE_STRPTIME CACHE)
	list(APPEND CMAKE_REQUIRED_DEFINITIONS -D${TIME_H_DEFINITIONS_TRY})
	check_symbol_exists(strptime time.h HAVE_STRPTIME)
	list(REMOVE_ITEM CMAKE_REQUIRED_DEFINITIONS -D${TIME_H_DEFINITIONS_TRY})
	if(HAVE_STRPTIME)
		# set as cache entry to persist across multiple builds
		set(TIME_H_DEFINITIONS ${TIME_H_DEFINITIONS_TRY} CACHE INTERNAL "Definitions required for time.h")
	endif()
endif()
check_symbol_exists(timegm time.h HAVE_TIMEGM)
if(NOT HAVE_TIMEGM)
	unset(HAVE_TIMEGM CACHE)
	list(APPEND CMAKE_REQUIRED_DEFINITIONS -D${TIME_H_DEFINITIONS_TRY})
	check_symbol_exists(timegm time.h HAVE_TIMEGM)
	list(REMOVE_ITEM CMAKE_REQUIRED_DEFINITIONS -D${TIME_H_DEFINITIONS_TRY})
	if(HAVE_TIMEGM)
		# set as cache entry to persist across multiple builds
		set(TIME_H_DEFINITIONS ${TIME_H_DEFINITIONS_TRY} CACHE INTERNAL "Definitions required for time.h")
	endif()
endif()
if(WIN32)
	check_symbol_exists(_mkgmtime time.h HAVE_MKGMTIME)
endif()
check_symbol_exists(localtime_r time.h HAVE_LOCALTIME_R)
if(NOT HAVE_LOCALTIME_R)
	unset(HAVE_LOCALTIME_R CACHE)
	list(APPEND CMAKE_REQUIRED_DEFINITIONS -D${TIME_H_DEFINITIONS_TRY})
	check_symbol_exists(localtime_r time.h HAVE_LOCALTIME_R)
	list(REMOVE_ITEM CMAKE_REQUIRED_DEFINITIONS -D${TIME_H_DEFINITIONS_TRY})
	if(HAVE_LOCALTIME_R)
		# set as cache entry to persist across multiple builds
		set(TIME_H_DEFINITIONS ${TIME_H_DEFINITIONS_TRY} CACHE INTERNAL "Definitions required for time.h")
	endif()
endif()
check_symbol_exists(gmtime_r time.h HAVE_GMTIME_R)
if(NOT HAVE_GMTIME_R)
	unset(HAVE_GMTIME_R CACHE)
	list(APPEND CMAKE_REQUIRED_DEFINITIONS -D${TIME_H_DEFINITIONS_TRY})
	check_symbol_exists(gmtime_r time.h HAVE_GMTIME_R)
	list(REMOVE_ITEM CMAKE_REQUIRED_DEFINITIONS -D${TIME_H_DEFINITIONS_TRY})
	if(HAVE_GMTIME_R)
		# set as cache entry to persist across multiple builds
		set(TIME_H_DEFINITIONS ${TIME_H_DEFINITIONS_TRY} CACHE INTERNAL "Definitions required for time.h")
	endif()
endif()
# Only setlocale() and either timegm() or _mkgmtime() are required for
# date/time conversion. If strptime() is absent, sscanf() will be used instead.
if(HAVE_SETLOCALE AND (HAVE_TIMEGM OR HAVE_MKGMTIME))
	message(STATUS "Enabling date/time conversion")
	# set as cache entry to persist across multiple builds
	set(TR31_ENABLE_DATETIME_CONVERSION ON CACHE INTERNAL "Date/time conversion availability")
else()
	message(STATUS "Disabling date/time conversion; some tests may fail")
	# set as cache entry to persist across multiple builds
	set(TR31_ENABLE_DATETIME_CONVERSION OFF CACHE INTERNAL "Date/time conversion availability")
endif()
# option to use sscanf() for date/time parsing even when strptime() is
# available
option(TR31_USE_SSCANF_DATETIME "Use sscanf() for date/time parsing")
if(HAVE_STRPTIME AND NOT TR31_USE_SSCANF_DATETIME)
	message(STATUS "Using strptime() for date/time parsing")
else()
	message(STATUS "Using sscanf() for date/time parsing")
endif()

include(GNUInstallDirs) # provides CMAKE_INSTALL_* variables and good defaults for install()

# generate config file for internal use only
# this file should NOT be installed or used by an installed header
configure_file(
	tr31_config.h.in
	tr31_config.h
)

# TR-31 library
add_library(tr31
	tr31.c
	tr31_crypto.c
	tr31_strings.c
)
if(TIME_H_DEFINITIONS)
	set_source_files_properties(tr31_strings.c
		PROPERTIES
			COMPILE_DEFINITIONS ${TIME_H_DEFINITIONS}
	)
endif()
set_target_properties(tr31
	PROPERTIES
		PUBLIC_HEADER tr31.h
		VERSION ${CMAKE_PROJECT_VERSION}
		SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}
)
target_include_directories(tr31 PRIVATE ${CMAKE_CURRENT_BINARY_DIR}) # for generated config file
target_include_directories(tr31 INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)
target_link_libraries(tr31 PRIVATE crypto_tdes crypto_aes crypto_mem crypto_rand)
if(HAVE_WINSOCK_H AND NOT HAVE_ARPA_INET_H)
	target_link_libraries(tr31 PRIVATE ws2_32)
endif()

install(
	TARGETS
		crypto_tdes
		crypto_aes
		crypto_mem
		crypto_rand
		tr31
	EXPORT tr31Targets # for use by install(EXPORT) command
	PUBLIC_HEADER
		DESTINATION "include/${PROJECT_NAME}"
		COMPONENT tr31_development
	RUNTIME
		COMPONENT tr31_runtime
	LIBRARY
		COMPONENT tr31_runtime
		NAMELINK_COMPONENT tr31_development
	ARCHIVE
		COMPONENT tr31_development
)

# TR-31 command line tool
if(BUILD_TR31_TOOL)
	add_executable(tr31-tool tr31-tool.c)
	if(HAVE_GMTIME_R AND TIME_H_DEFINITIONS)
		set_source_files_properties(tr31-tool.c
			PROPERTIES
				COMPILE_DEFINITIONS ${TIME_H_DEFINITIONS}
		)
	endif()
	target_include_directories(tr31-tool PRIVATE ${CMAKE_CURRENT_BINARY_DIR}) # for generated config file
	target_link_libraries(tr31-tool PRIVATE crypto_mem tr31)
	if(TARGET libargp::argp)
		target_link_libraries(tr31-tool PRIVATE libargp::argp)
	endif()

	install(
		TARGETS
			tr31-tool
		EXPORT tr31Targets # for use by install(EXPORT) command
		RUNTIME
			COMPONENT tr31_runtime
	)

	add_executable(tr31-kbpk tr31-kbpk.c)
	target_include_directories(tr31-kbpk PRIVATE ${CMAKE_CURRENT_BINARY_DIR}) # for generated config file
	target_link_libraries(tr31-kbpk PRIVATE crypto_mem tr31)
	if(TARGET libargp::argp)
		target_link_libraries(tr31-kbpk PRIVATE libargp::argp)
	endif()

	install(
		TARGETS
			tr31-kbpk
		EXPORT tr31Targets # for use by install(EXPORT) command
		RUNTIME
			COMPONENT tr31_runtime
	)

	add_executable(tr31-kcv tr31-kcv.c)
	target_include_directories(tr31-kcv PRIVATE ${CMAKE_CURRENT_BINARY_DIR}) # for generated config file
	target_link_libraries(tr31-kcv PRIVATE crypto_tdes crypto_aes crypto_mem tr31)
	if(TARGET libargp::argp)
		target_link_libraries(tr31-kcv PRIVATE libargp::argp)
	endif()

	install(
		TARGETS
			tr31-kcv
		EXPORT tr31Targets # for use by install(EXPORT) command
		RUNTIME
			COMPONENT tr31_runtime
	)

	add_executable(tr31-mac tr31-mac.c)
	target_include_directories(tr31-mac PRIVATE ${CMAKE_CURRENT_BINARY_DIR}) # for generated config file
	target_link_libraries(tr31-mac PRIVATE crypto_tdes crypto_aes crypto_mem tr31)
	if(TARGET libargp::argp)
		target_link_libraries(tr31-mac PRIVATE libargp::argp)
	endif()

	install(
		TARGETS
			tr31-mac
		EXPORT tr31Targets # for use by install(EXPORT) command
		RUNTIME
			COMPONENT tr31_runtime
	)

	add_executable(tr31-crypt tr31-crypt.c)
	target_include_directories(tr31-crypt PRIVATE ${CMAKE_CURRENT_BINARY_DIR}) # for generated config file
	target_link_libraries(tr31-crypt PRIVATE crypto_tdes crypto_aes crypto_mem tr31)
	if(TARGET libargp::argp)
		target_link_libraries(tr31-crypt PRIVATE libargp::argp)
	endif()

	install(
		TARGETS
			tr31-crypt
		EXPORT tr31Targets # for use by install(EXPORT) command
		RUNTIME
			COMPONENT tr31_runtime
	)

	add_executable(tr31-header tr31-header.c)
	target_include_directories(tr31-header PRIVATE ${CMAKE_CURRENT_BINARY_DIR}) # for generated config file
	target_link_libraries(tr31-header PRIVATE tr31)
	if(TARGET libargp::argp)
		target_link_libraries(tr31-header PRIVATE libargp::argp)
	endif()

	install(
		TARGETS
			tr31-header
		EXPORT tr31Targets # for use by install(EXPORT) command
		RUNTIME
			COMPONENT tr31_runtime
	)
endif()

if(TARGET tr31-tool AND BUILD_TESTING)
	add_test(NAME tr31_tool_test1
		COMMAND tr31-tool --import B0128B1TX00N0300KS18FFFF00A0200001E00000KC0C000169E3KP0C00ECAD626F9F1A826814AA066D86C8C18BD0E14033E1EBEC75BEDF586E6E325F3AA8C0E5 --kbpk AB2E09DB3EF0BA71E0CE6CD755C23A3B
	)
	string(CONCAT tr31_tool_test1_regex
		"^Key block format version: B[\r\n]"
		"Key block length: 128 bytes[\r\n]"
		"Key usage: \\[B1\\] Initial DUKPT Key \\(IK/IPEK\\)[\r\n]"
		"Key algorithm: \\[T\\] TDES[\r\n]"
		"Key mode of use: \\[X\\] Key Derivation[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Optional blocks \\[3\\]:[\r\n]"
		"\t\\[KS\\] Initial Key Serial Number \\(KSN\\): FFFF00A0200001E00000[\r\n]"
		"\t\\[KC\\] Key Check Value \\(KCV\\) of wrapped key: 0169E3 \\(Legacy KCV algorithm\\)[\r\n]"
		"\t\\[KP\\] Key Check Value \\(KCV\\) of KBPK: ECAD62 \\(Legacy KCV algorithm\\)[\r\n]"
		"Key length: 16[\r\n]"
		"Key value: BF82DAC6A33DF92CE66E15B70E5DCEB6 \\(KCV: 0169E3\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test1_regex}
	)

	add_test(NAME tr31_tool_test2
		COMMAND tr31-tool --import D0112B0TN00N000037DB9B046B7B0048785690759580ABC3B9842AB4BB7717B49E92528E575785D8123559376A2553B27BE94F054F4E971C --kbpk 4141414141414141414141414141414141414141414141414141414141414141
	)
	string(CONCAT tr31_tool_test2_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 112 bytes[\r\n]"
		"Key usage: \\[B0\\] Base Derivation Key \\(BDK\\)[\r\n]"
		"Key algorithm: \\[T\\] TDES[\r\n]"
		"Key mode of use: \\[N\\] No special restrictions[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Key length: 16[\r\n]"
		"Key value: 1FA1F7CEC798D91545DA8AE0C7796BD9 \\(KCV: FF5087\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test2_regex}
	)

	add_test(NAME tr31_tool_test3
		COMMAND tr31-tool --import D0144B0AN00N0000DCD6D7E8108277E3304831A744F741A51A30695A2F764C6E42A1F54086FF0C3F5F9BEC889F20C6F613EF790A09381A5855B9464E598CBE24E537B6FE0F602297 --kbpk 4141414141414141414141414141414141414141414141414141414141414141
	)
	string(CONCAT tr31_tool_test3_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 144 bytes[\r\n]"
		"Key usage: \\[B0\\] Base Derivation Key \\(BDK\\)[\r\n]"
		"Key algorithm: \\[A\\] AES[\r\n]"
		"Key mode of use: \\[N\\] No special restrictions[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Key length: 32[\r\n]"
		"Key value: 6189A1111AAB43DC8BEAEEAE29799C4212070D66F8933EFACE8879C7DB609DEA \\(KCV: 69EBA40B13\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test3_regex}
	)

	add_test(NAME tr31_tool_test4
		COMMAND tr31-tool --import D0144B0AN00N000004E08A6000D7B21A5CFFE0F24E449A21D9BE3F975D33DB02D50CF9A6CCD2F6B6DB375CBCCCA648E0FB991432CA3957E810B087CDA7FA44538CC94C97FFF6D696 --kbpk 4141414141414141414141414141414141414141414141414141414141414141
	)
	string(CONCAT tr31_tool_test4_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 144 bytes[\r\n]"
		"Key usage: \\[B0\\] Base Derivation Key \\(BDK\\)[\r\n]"
		"Key algorithm: \\[A\\] AES[\r\n]"
		"Key mode of use: \\[N\\] No special restrictions[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Key length: 32[\r\n]"
		"Key value: 6189A1111AAB43DC8BEAEEAE29799C4212070D66F8933EFACE8879C7DB609DEA \\(KCV: 69EBA40B13\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test4
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test4_regex}
	)

	add_test(NAME tr31_tool_test5
		COMMAND tr31-tool --import D0144D0AN00N0000127862F945C2DED04530FAF7CDBC8B0BA10C7AA79BD5E0C2C5D6AC173BF588E4B19ACF1357178D50EA0AB193228E13958304FC6149632DFDCADF3A5B3D57E814 --kbpk 4141414141414141414141414141414141414141414141414141414141414141
	)
	string(CONCAT tr31_tool_test5_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 144 bytes[\r\n]"
		"Key usage: \\[D0\\] Symmetric Key for Data Encryption[\r\n]"
		"Key algorithm: \\[A\\] AES[\r\n]"
		"Key mode of use: \\[N\\] No special restrictions[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Key length: 32[\r\n]"
		"Key value: BE19E6A07A760F10EF8E83A226B63AAD141F463FDDD4F47DB244B4023EC3CACC \\(KCV: 0A00E31EEB\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test5
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test5_regex}
	)

	add_test(NAME tr31_tool_test6
		COMMAND tr31-tool --kbpk 1D22BF32387C600AD97F9B97A51311AC --export E8BC63E5479455E26577F715D587FE68 --export-key-algorithm TDES --export-format-version B --export-template IK
	)
	set_tests_properties(tr31_tool_test6
		PROPERTIES
			WILL_FAIL TRUE # because --export-opt-block-KS is absent
	)

	add_test(NAME tr31_tool_test7
		COMMAND tr31-tool --kbpk B8ED59E0A279A295E9F5ED7944FD06B9 --export EDB380DD340BC2620247D445F5B8D678 --export-key-algorithm TDES --export-format-version C --export-template BDK --export-no-key-length-obfuscation
	)
	set_tests_properties(tr31_tool_test7
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^C0072B0TX00E0000"
	)

	add_test(NAME tr31_tool_test8
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --export 3F419E1CB7079442AA37474C2EFBF8B8 --export-key-algorithm AES --export-format-version D --export-template KEK --export-no-key-length-obfuscation
	)
	set_tests_properties(tr31_tool_test8
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0112K0AB00E0000"
	)

	add_test(NAME tr31_tool_test9
		COMMAND tr31-tool --kbpk 1D22BF32387C600AD97F9B97A51311AC --export E8BC63E5479455E26577F715D587FE68 --export-header B0080B1TX00N0000 --export-no-key-length-obfuscation
	)
	set_tests_properties(tr31_tool_test9
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^B0080B1TX00N0000"
	)

	add_test(NAME tr31_tool_test10
		COMMAND tr31-tool --kbpk B8ED59E0A279A295E9F5ED7944FD06B9 --export EDB380DD340BC2620247D445F5B8D678 --export-header C0072B0TX00E0000 --export-no-key-length-obfuscation
	)
	set_tests_properties(tr31_tool_test10
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^C0072B0TX00E0000"
	)

	add_test(NAME tr31_tool_test11
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --export 3F419E1CB7079442AA37474C2EFBF8B8 --export-header D0112K0AB00E0000 --export-no-key-length-obfuscation
	)
	set_tests_properties(tr31_tool_test11
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0112K0AB00E0000"
	)

	add_test(NAME tr31_tool_test12
		COMMAND tr31-tool --kbpk 1D22BF32387C600AD97F9B97A51311AC --export E8BC63E5479455E26577F715D587FE68 --export-key-algorithm TDES --export-format-version B --export-template IK --export-opt-block-KS FFFF00A0200001E00000 --export-no-key-length-obfuscation
	)
	set_tests_properties(tr31_tool_test12
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^B0104B1TX00N0100KS18FFFF00A0200001E00000"
	)

	add_test(NAME tr31_tool_test13
		COMMAND tr31-tool --kbpk 1D22BF32387C600AD97F9B97A51311AC --export E8BC63E5479455E26577F715D587FE68 --export-header B0000B1TX00N0000 --export-opt-block-KS FFFF00A0200001E00000 --export-no-key-length-obfuscation
	)
	set_tests_properties(tr31_tool_test13
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^B0104B1TX00N0100KS18FFFF00A0200001E00000"
	)

	add_test(NAME tr31_tool_test14
		COMMAND tr31-tool --kbpk B8ED59E0A279A295E9F5ED7944FD06B9 --export EDB380DD340BC2620247D445F5B8D678 --export-key-algorithm TDES --export-format-version C --export-template BDK --export-opt-block-KC --export-opt-block-KP --export-no-key-length-obfuscation
	)
	set_tests_properties(tr31_tool_test14
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^C0096B0TX00E0200KC0C00F4B08DKP0C002F0EEA"
	)

	add_test(NAME tr31_tool_test15
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --export 3F419E1CB7079442AA37474C2EFBF8B8 --export-key-algorithm AES --export-format-version D --export-template KEK --export-opt-block-KC --export-opt-block-KP --export-no-key-length-obfuscation
	)
	set_tests_properties(tr31_tool_test15
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0144K0AB00E0200KC100108793E25ABKP10012331550BC9"
	)

	add_test(NAME tr31_tool_test16
		COMMAND tr31-tool --kbpk 4141414141414141414141414141414141414141414141414141414141414141 --export 1273671EA26AC29AFA4D1084127652A1 --export-key-algorithm AES --export-format-version D --export-template IK --export-opt-block-IK 1234567890123456 --export-no-key-length-obfuscation --export-zero-opt-block-PB
	)
	set_tests_properties(tr31_tool_test16
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0144B1AX00N0200IK141234567890123456PB0C00000000"
	)

	add_test(NAME tr31_tool_test17
		COMMAND tr31-tool --kbpk 4141414141414141414141414141414141414141414141414141414141414141 --export 1273671EA26AC29AFA4D1084127652A1 --export-header D0000B1AX00N0000 --export-opt-block-IK 1234567890123456 --export-no-key-length-obfuscation --export-zero-opt-block-PB
	)
	set_tests_properties(tr31_tool_test17
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0144B1AX00N0200IK141234567890123456PB0C00000000"
	)

	add_test(NAME tr31_tool_test18
		COMMAND tr31-tool --kbpk 3235362D62697420414553207772617070696E67202849534F20323030333829 --export 777261707065642033444553206B6579 --export-header E0084B0TV16N0000 --export-no-key-length-obfuscation
	)
	set_tests_properties(tr31_tool_test18
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^E0084B0TV16N0000B2AE5E26BBA7F246E84D5EA24167E208A6B66EF2E27E55A52DB52F0AEACB94C57547"
	)

	add_test(NAME tr31_tool_test19
		COMMAND tr31-tool --kbpk 3235362D62697420414553207772617070696E67202849534F20323030333829 --export 777261707065642033444553206B6579 --export-key-algorithm TDES --export-format-version E --export-template BDK --export-no-key-length-obfuscation
	)
	set_tests_properties(tr31_tool_test19
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^E0084B0TX00E000091549081F898337CE9C68236C1349B6151AB4B48F77C61348C94CD65537306BEE1C5"
	)

	add_test(NAME tr31_tool_test20
		COMMAND tr31-tool --import D0144B1TX00S0200KS181ED9D500000001200000PB08nD8U3CAC6F42EF3E1711310E8BC2738E30211D3A1AB6EA61254CE37B5C162C743D1DAB6C866E69354E0276D7E744AAFB3425 --kbpk 0123456789ABCDEFFEDCBA9876543210
	)
	string(CONCAT tr31_tool_test20_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 144 bytes[\r\n]"
		".*"
		"Optional blocks \\[2\\]:[\r\n]"
		"\t\\[KS\\] Initial Key Serial Number \\(KSN\\): 1ED9D500000001200000[\r\n]"
		"\t\\[PB\\] Padding Block: \"nD8U\"[\r\n]"
		"Key length: 16[\r\n]"
		"Key value: 4AC3A7637268E618C52C97CE7766F26A \\(KCV: 72098B\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test20
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test20_regex}
	)

	add_test(NAME tr31_tool_test21
		COMMAND tr31-tool --kbpk 0123456789ABCDEFFEDCBA9876543210 --export 4AC3A7637268E618C52C97CE7766F26A --export-header D0000D0TB00S0000 --export-opt-block-TC 20190717181929Z --export-opt-block-TS 20200818192030Z
	)
	set_tests_properties(tr31_tool_test21
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0160D0TB00S0300TC1320190717181929ZTS1320200818192030ZPB0A"
	)

	add_test(NAME tr31_tool_test22
		COMMAND tr31-tool --kbpk 0123456789ABCDEFFEDCBA9876543210 --export 4AC3A7637268E618C52C97CE7766F26A --export-header D0000D0TB00S0000 --export-opt-block-TC 2019071718192921Z --export-opt-block-TS 2020081819203022Z
	)
	set_tests_properties(tr31_tool_test22
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0160D0TB00S0300TC152019071718192921ZTS152020081819203022ZPB06"
	)

	add_test(NAME tr31_tool_test23
		COMMAND tr31-tool --kbpk 0123456789ABCDEFFEDCBA9876543210 --export 4AC3A7637268E618C52C97CE7766F26A --export-header D0000D0TB00S0000 --export-opt-block-TC 2017-05-17T19:41:38Z --export-opt-block-TS 2018-06-18T20:42:39Z
	)
	set_tests_properties(tr31_tool_test23
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0160D0TB00S0200TC182017-05-17T19:41:38ZTS182018-06-18T20:42:39"
	)

	add_test(NAME tr31_tool_test24
		COMMAND tr31-tool --kbpk 0123456789ABCDEFFEDCBA9876543210 --export 4AC3A7637268E618C52C97CE7766F26A --export-header D0000D0TB00S0000 --export-opt-block-TC 2017-05-17T19:41:38.21Z --export-opt-block-TS 2018-06-18T20:42:39.22Z
	)
	set_tests_properties(tr31_tool_test24
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0176D0TB00S0300TC1B2017-05-17T19:41:38.21ZTS1B2018-06-18T20:42:39.22ZPB0A"
	)

	add_test(NAME tr31_tool_test25
		COMMAND tr31-tool --kbpk 0123456789ABCDEFFEDCBA9876543210 --import D0160D0TB00S0300TC1320190717181929ZTS1320200818192030ZPB0AMn6Rg3B31D549F386D00422AA0C5BA5DAF057C1AA495ACC79AF92500D381AD3F3462F927DDA9DCFB6158C33B5D63CA08791F76
	)
	string(CONCAT tr31_tool_test25_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 160 bytes[\r\n]"
		".*"
		"Optional blocks \\[3\\]:[\r\n]"
		"\t\\[TC\\] Time of Creation: \"20190717181929Z\" \\(.*\\)[\r\n]"
		"\t\\[TS\\] Time Stamp: \"20200818192030Z\" \\(.*\\)[\r\n]"
		"\t\\[PB\\] Padding Block: \"Mn6Rg3\"[\r\n]"
		"Key length: 16[\r\n]"
		"Key value: 4AC3A7637268E618C52C97CE7766F26A \\(KCV: 72098B\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test25
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test25_regex}
	)

	add_test(NAME tr31_tool_test26
		COMMAND tr31-tool --kbpk 0123456789ABCDEFFEDCBA9876543210 --import D0160D0TB00S0300TC152019071718192921ZTS152020081819203022ZPB063r3AFB97760D5C46D3EE774A00FA8C1393F259B5862F89AC477F66560B593A2CE20EE5638FE26717B7DB067D508B2C3A98
	)
	string(CONCAT tr31_tool_test26_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 160 bytes[\r\n]"
		".*"
		"Optional blocks \\[3\\]:[\r\n]"
		"\t\\[TC\\] Time of Creation: \"2019071718192921Z\" \\(.*\\)[\r\n]"
		"\t\\[TS\\] Time Stamp: \"2020081819203022Z\" \\(.*\\)[\r\n]"
		"\t\\[PB\\] Padding Block: \"3r\"[\r\n]"
		"Key length: 16[\r\n]"
		"Key value: 4AC3A7637268E618C52C97CE7766F26A \\(KCV: 72098B\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test26
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test26_regex}
	)

	add_test(NAME tr31_tool_test27
		COMMAND tr31-tool --kbpk 0123456789ABCDEFFEDCBA9876543210 --import D0160D0TB00S0200TC182017-05-17T19:41:38ZTS182018-06-18T20:42:39Z8334D53013CF72F36AC047C75ADDB106075ACD7F179C07D21D0DCE06D2F507BC16FA0E3C2C2A5266A55D220F35A3C920
	)
	string(CONCAT tr31_tool_test27_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 160 bytes[\r\n]"
		".*"
		"Optional blocks \\[2\\]:[\r\n]"
		"\t\\[TC\\] Time of Creation: \"2017-05-17T19:41:38Z\" \\(.*\\)[\r\n]"
		"\t\\[TS\\] Time Stamp: \"2018-06-18T20:42:39Z\" \\(.*\\)[\r\n]"
		"Key length: 16[\r\n]"
		"Key value: 4AC3A7637268E618C52C97CE7766F26A \\(KCV: 72098B\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test27
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test27_regex}
	)

	add_test(NAME tr31_tool_test28
		COMMAND tr31-tool --kbpk 0123456789ABCDEFFEDCBA9876543210 --import D0176D0TB00S0300TC1B2017-05-17T19:41:38.21ZTS1B2018-06-18T20:42:39.22ZPB0A0tGy4eABD31E32E882F6BC6E198890F6EDA720396691AC57710BBC6CF22CC164A1E80D06FE38EE315657DD0DA9ED1EDC6754B2
	)
	string(CONCAT tr31_tool_test28_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 176 bytes[\r\n]"
		".*"
		"Optional blocks \\[3\\]:[\r\n]"
		"\t\\[TC\\] Time of Creation: \"2017-05-17T19:41:38.21Z\" \\(.*\\)[\r\n]"
		"\t\\[TS\\] Time Stamp: \"2018-06-18T20:42:39.22Z\" \\(.*\\)[\r\n]"
		"\t\\[PB\\] Padding Block: \"0tGy4e\"[\r\n]"
		"Key length: 16[\r\n]"
		"Key value: 4AC3A7637268E618C52C97CE7766F26A \\(KCV: 72098B\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test28
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test28_regex}
	)

	add_test(NAME tr31_tool_test29
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --export 3F419E1CB7079442AA37474C2EFBF8B8 --export-key-algorithm AES --export-format-version D --export-template BDK --export-opt-block-BI 08793E25 --export-opt-block-KC --export-opt-block-KP
	)
	set_tests_properties(tr31_tool_test29
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0208B0AX00E0400BI0E0108793E25KC100108793E25ABKP10012331550BC9PB12"
	)

	add_test(NAME tr31_tool_test30
		COMMAND tr31-tool --kbpk B8ED59E0A279A295E9F5ED7944FD06B9 --import B0136B0TX00E0300BI100008793E2500KC0C00F4B08DKP0C002F0EEA16BD28BCC923ED336AF77733257CD5238E406F957CEDFE9172E7B2C88377EAC6884223A69E9B499D
	)
	string(CONCAT tr31_tool_test30_regex
		"^Key block format version: B[\r\n]"
		"Key block length: 136 bytes[\r\n]"
		"Key usage: \\[B0\\] Base Derivation Key \\(BDK\\)[\r\n]"
		"Key algorithm: \\[T\\] TDES[\r\n]"
		"Key mode of use: \\[X\\] Key Derivation[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[E\\] Exportable in a trusted key block only[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Optional blocks \\[3\\]:[\r\n]"
		"\t\\[BI\\] Base Derivation Key \\(BDK\\) Identifier: 08793E2500 \\(Key Set ID\\)[\r\n]"
		"\t\\[KC\\] Key Check Value \\(KCV\\) of wrapped key: F4B08D \\(Legacy KCV algorithm\\)[\r\n]"
		"\t\\[KP\\] Key Check Value \\(KCV\\) of KBPK: 2F0EEA \\(Legacy KCV algorithm\\)[\r\n]"
		"Key length: 16[\r\n]"
		"Key value: EDB380DD340BC2620247D445F5B8D678 \\(KCV: F4B08D\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test30
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test30_regex}
	)

	add_test(NAME tr31_tool_test31
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --export 3F419E1CB7079442AA37474C2EFBF8B8 --export-header D0000B3AX00E0000 --export-opt-block-DA D2ABND3AEN --export-opt-block-LB "My Great Key" --export-opt-block-PK 9876543210 --export-opt-block-WP 3
	)
	set_tests_properties(tr31_tool_test31
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0208B3AX00E0500DA1001D2ABND3AENLB10My Great KeyPK10019876543210WP07003PB09"
	)

	add_test(NAME tr31_tool_test32
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --import "D0208B3AX00E0500DA1001D2ABND3AENLB10My Great KeyPK10019876543210WP07003PB09AcsovCCFF2ACD2116AB1CD7923B7C1AC7DD614EAC0EB7B6FE94BE3576D985E9E2AD3AF493FCB1B5CA34C51AB7A84D2C98DEAB51C6C51F82BFAD1DD1A82EB14C8FEB85"
	)
	string(CONCAT tr31_tool_test32_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 208 bytes[\r\n]"
		"Key usage: \\[B3\\] Key Derivation Key[\r\n]"
		"Key algorithm: \\[A\\] AES[\r\n]"
		"Key mode of use: \\[X\\] Key Derivation[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[E\\] Exportable in a trusted key block only[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Optional blocks \\[5\\]:[\r\n]"
		"\t\\[DA\\] Derivation\\(s\\) Allowed for Derivation Keys: D2ABN,D3AEN[\r\n]"
		"\t\\[LB\\] Label: \"My Great Key\"[\r\n]"
		"\t\\[PK\\] Protection Key Check Value \\(KCV\\) of export KBPK: 9876543210 \\(CMAC based KCV\\)[\r\n]"
		"\t\\[WP\\] Wrapping Pedigree: 3 \\(Asymmetric key at risk of quantum computing and symmetric key of lesser effective strength\\)[\r\n]"
		"\t\\[PB\\] Padding Block: \"Acsov\"[\r\n]"
		"Key length: 16[\r\n]"
		"Key value: 3F419E1CB7079442AA37474C2EFBF8B8 \\(KCV: 08793E25AB\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test32
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test32_regex}
	)

	# test HMAC key for ANSI X9.143
	add_test(NAME tr31_tool_test33
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --export 3F419E1CB7079442AA37474C2EFBF8B8 --export-header D0000M7HG00N0000 --export-opt-block-HM 21
	)
	set_tests_properties(tr31_tool_test33
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0128M7HG00N0200HM0621PB0A"
	)

	# test HMAC key for ANSI X9.143
	add_test(NAME tr31_tool_test34
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --import D0128M7HG00N0200HM0621PB0A5V5E8F634619D84C6040EAC3C1CA6A5FE60C5292E778FC95ABBECE94C3AD060C5F4C128796A100FF035A4BFB16C36449550131
	)
	string(CONCAT tr31_tool_test34_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 128 bytes[\r\n]"
		"Key usage: \\[M7\\] HMAC Key[\r\n]"
		"Key algorithm: \\[H\\] HMAC[\r\n]"
		"Key mode of use: \\[G\\] MAC Generate Only[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Optional blocks \\[2\\]:[\r\n]"
		"\t\\[HM\\] Hash algorithm for HMAC: 21 \\(SHA-256\\)[\r\n]"
		"\t\\[PB\\] Padding Block: \"5V5E8F\"[\r\n]"
		"Key length: 16[\r\n]"
		"Key value: 3F419E1CB7079442AA37474C2EFBF8B8[\r\n]"
	)
	set_tests_properties(tr31_tool_test34
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test34_regex}
	)

	# ANSI X9.143:2021, 8.5
	add_test(NAME tr31_tool_test35
		COMMAND tr31-tool --kbpk FA36E44278DB3AB5F298F9F7DA8F1F88 --import D3776S0RS00N0400CT0002050000MIIDszCCApugAwIBAgIIKpD5FKMfCZEwDQYJKoZIhvcNAQELBQAwLTEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxEjAQBgNVBAMMCVNhbXBsZSBDQTAeFw0yMDA4MTUwMjE0MTBaFw0yMTA4MTUwMjE0MTBaME8xFzAVBgNVBAoMDkFscGhhIE1lcmNoYW50MR8wHQYDVQQLDBZUTFMgQ2xpZW50IENlcnRpZmljYXRlMRMwEQYDVQQDDAoxMjM0NTY3ODkwMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA1sRg+wEuje3y14V0tFHpvxxpY/fyrldB0nRctBDn4AvkBfyJuDLG59vqkGXVd8J8YQdwEHZJrVq+7B8rjtM6PMoyH/7QAZZAC7tw740P4cfen1IryubZVviV9QUp+gHToelZfr1rfIsuEGhzo6UhwY70kkS87/rYHCVathZEjMmvUIEdpzg0PZ2+Heg6D35OQ70I+np+BsEQf71Zr+d2iKqVGEd50l8tbn4W3A4rOyUERPTaACwS9rvdF7nlmTqSI5ybN6lmm37a71h77n6M54aaw2KkJYWVo+1stUTyFVsv/YBs9aylbBHQOYqp/U2tB0TxM58QYGzyaWvNqbFzOQIDAQABo4G0MIGxMAkGA1UdEwQCMAAwDgYDVR0PAQH/BAQDAgeAMBMGA1UdJQQMMAoGCCsGAQUFBwMCMB0GA1UdDgQWBBR837QRAGx5uL9xDnRjr9L9WSBSlzAfBgNVHSMEGDAWgBSlXhVYy9bic9OLnRsxsFgKQQbLmTA/BgNVHR8EODA2MDSgMqAwhi5odHRwOi8vY3JsLmFscGhhLW1lcmNoYW50LmV4YW1wbGUvU2FtcGxlQ0EuY3JsMA0GCSqGSIb3DQEBCwUAA4IBAQCH6JusIBSkRDqzAohaSoJVAEwQGMdcUSQWDfMyJjZqkOep1kT8Sl7LolFmmmVRJdkTWZe4PxBfQUc/eIql9BIx90506B+j9aoVA7212OExAid78GgqKA6JoalhYQKRta9ixY8iolydTYyEYpegA1jFZavMQma4ZGwX/bDJWr4+cJYxJXWaf67g4AMqHaWC8J60MVjrrBe9BZ0ZstuIlNkktQUOZanqxqsrFeqz02ibwTwNHtaHQCztB4KgdTkrTNahkqeq6xjafDoTllNo1EddajnbA/cVzF9ZCNigDtg5chXHWIQbgEK7HmU3sY3/wd2Bh1KdF3+vpN+5iZMRNv7ZKP1001D77F007724TS1320200818221218ZPB0D0000000006A5FE664385E390C880D12C1E81B74BD0C35D00A27085602753679E3F00FD00B953558F47F972F0A3CF03DA5F75EBAAE21DA27AFF0D1B8CD213F036635BAD24DE82DB1832C245BB38EE40BD530FF5BFFF3C35131AE1EAD3871E6569114BEBC8CE462AE992B5AC0F33C3E995FDB0225691CDF01BF695EC96F5442AB9650C22896E16C368C1FB796CCA72CA1CF2D403DB1D30A0D63E7E92D61A3A438C33C529E99C8036B6D2015F4693DD5C7949F6BC12AE6E33C2EED215DBB1E9CD6D8528B3F02B7108B568AF252C6F26C599BEEDB4F81A5FEF6E2F2D8888D327F15A8CED909C5D84061FC8F62A021CADA7C1D7B241EE5FB3FC359CED864523CB64E75BEC4262E5A31E93032DF64000D717D3AD78D0A62FE00243B76D1D3A39290EE9C980C9A38F33CF26B5FA7359EC396B8AD8D2945965110A6C35294797099D39827C62CB028E35E763FE69EE3C0D0E19A9B7520DA44B79ED18289C354E7BE339E567D5C80921033CF950955CAED0E0FB739A1DD793DAD7BCB3F0BA068AF8EEBFD771B0D8E86953F1B7A81150D7953BDFC70E760E187E3D4DAD57F179B3016B5032F12B6C227E3FDD3000D5198100AD95FC6B45EFFD0AD2772242D7EA6D73F186F391CA1777EDF9A78ACF4C62807DF4B8A43E91A519DC67E576D7B0ADC2D16C7FBFE6F67B05A484EC58DB7F8129FC7CAE61A158BB391A12EFCCF84D25791485B4A5CFDA75133EED967DEA9BDD1CC32A8D5A18ACE53A9C607EC8491C80335231B1CB77AAAAC3D113B21DD366CB2FF13A97961E13E48A5B1CA0A70026A7DD2F47529D7EC8055A0516043D6481DFC69A4779B13040C3E6FE8A4B73DD14306177BDC65DAFE6CD5BA6FD3A0071A7ACFF0BFED47784BEEF8E11D5398E704A6DC9CC99B22E8424B27D1C575373B0387FAA7140D344E3409235C16F478EC819CA11931EC6F4344D4C34A9B5E6F8617392A4E1DC625BEF83C767B135244FC75473F5CEAA44E2BE6C42A6AC0DA232184D0428DE8D62D888D62C28739931AF64A4493BEA1293A124C90AA9D00F19460C5CAD47FA37DBF1481835EE9B1A597E40C7023F825DE15BE57CD75BD6BEA55F12C0372B59D623C898B24957CDFA0FFF88751E765B9D63EABA200A9917B148A1E776FA362F7B5856381303315C02936C56CDDADE81840D05D4C900C5D3A5A76BA0A8447F2E7B86000AEC27037EBF254ADC0FABDBC113A09316308374940968193259C2DDFBE0B7FB7D090896CCC94E058BD835230E9BD3B70E5BC13510382F831DEF5D86CBD5885FED9F4B77C69EF0EFF87FE90EC619971C4CF4532FEE7A07EA8ABAEF7112CC8DE75448CA4AD145B8D763F8409CCB143D5F05B2D6CBFC64299883DE5F5E14AE9D53D46D50088402E8B36E2BDF276EA209657391EBD2AE28D1FFDE7C5F1DA5F455A830BFA59B60FB755ABF4964FA9E553D7D5565D8E69B1F42EA7E5C40896F29F14F8394C43A4CD62F89BDCDF28ED9DC293429EEBE63CEC0C8984DC650268AFC4F663FB457C35EA862F96076226D793B29095CE4FE2B91CA6543F3336EA1C7A8A1EDFD3FA375CEE82811FAB190483B2D2DDEB04C34E165B9444EE2AD52387ADBE9D92893ADC96B01B03C2AE026BDFB3DF90DBD8A3774C17BBBF8F1E6541EBDFE522D2EA9BD26EFC02C4674E6AB0B0321F8EBD3F5FAD035B0A7C4DDCAB24CE8A0F913FE1AA3B03654351B97C511BD2B6B6FA69E7A22CD5
	)
	string(CONCAT tr31_tool_test35_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 3776 bytes[\r\n]"
		"Key usage: \\[S0\\] Asymmetric Key Pair for Digital Signature[\r\n]"
		"Key algorithm: \\[R\\] RSA[\r\n]"
		"Key mode of use: \\[S\\] Signature Only[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Optional blocks \\[4\\]:[\r\n]"
		"\t\\[CT\\] Public Key Certificate: \"MIIDszCCApugAwIB.*F3\\+vpN\\+5iZMRNv7Z\" \\(X\\.509 certificate\\)[\r\n]"
		"\t\\[KP\\] Key Check Value \\(KCV\\) of KBPK: D77F007724 \\(CMAC based KCV\\)[\r\n]"
		"\t\\[TS\\] Time Stamp: \"20200818221218Z\" \\(.*\\)[\r\n]"
		"\t\\[PB\\] Padding Block: \"000000000\"[\r\n]"
		"Key length: 1192[\r\n]"
		"Key value: 308204A402010002.*1641B00E23636A68[\r\n]"
	)
	set_tests_properties(tr31_tool_test35
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test35_regex}
	)

	# ANSI X9.143:2021, 8.6
	add_test(NAME tr31_tool_test36
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --import D1840S0ES00N0400CT000205CC020002F0MIICLjCCAdSgAwIBAgIIGDrdWBxuNpAwCgYIKoZIzj0EAwIwMTEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxFjAUBgNVBAMMDVNhbXBsZSBFQ0MgQ0EwHhcNMjAwODE1MDIxMDEwWhcNMjEwODE1MDIxMDEwWjBPMRcwFQYDVQQKDA5BbHBoYSBNZXJjaGFudDEfMB0GA1UECwwWVExTIENsaWVudCBDZXJ0aWZpY2F0ZTETMBEGA1UEAwwKMTIzNDU2Nzg5MDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABEI/SLrH6fITA9y6Y3BneuoT/5+EHSepZxCYeSstGll2sVvmSDZWWSbN6lh5Fb/zagrDjjQ/gZtWIOTf2wL1vSGjgbcwgbQwCQYDVR0TBAIwADAOBgNVHQ8BAf8EBAMCB4AwEwYDVR0lBAwwCgYIKwYBBQUHAwIwHQYDVR0OBBYEFHuvP526vFMywEoVoXZ5aXNfhnfeMB8GA1UdIwQYMBaAFI+ZFhOWF+oMtcfYwg15vH5WmWccMEIGA1UdHwQ7MDkwN6A1oDOGMWh0dHA6Ly9jcmwuYWxwaGEtbWVyY2hhbnQuZXhhbXBsZS9TYW1wbGVFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSAAwRQIhAPuWWvCTmOdvQzUjCUmTX7H4sX4Ebpw+CI+aOQLu1DqwAiA0eR4FdMtvXV4P6+WMz5B10oea5xtLTfSgoBDoTkvKYQ==0002C4MIICDjCCAbOgAwIBAgIIfnOsCbsxHjwwCgYIKoZIzj0EAwIwNjEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxGzAZBgNVBAMMElNhbXBsZSBSb290IEVDQyBDQTAeFw0yMDA4MTUwMjEwMDlaFw0zMDA4MTMwMjEwMDlaMDExFzAVBgNVBAoMDkFscGhhIE1lcmNoYW50MRYwFAYDVQQDDA1TYW1wbGUgRUNDIENBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEHCanM9n+Rji+3EROj+HlogmXMU1Fk1td7N3I/8rfFnre1GwWCUqXSePHxwQ9DRHCV3oht3OUU2kDfitfUIujA6OBrzCBrDASBgNVHRMBAf8ECDAGAQH/AgEAMA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUj5kWE5YX6gy1x9jCDXm8flaZZxwwHwYDVR0jBBgwFoAUvElIifFlt6oeUaopV9Y0lJtyPVQwRgYDVR0fBD8wPTA7oDmgN4Y1aHR0cDovL2NybC5hbHBoYS1tZXJjaGFudC5leGFtcGxlL1NhbXBsZVJvb3RFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSQAwRgIhALT8+DG+++KuqqUGyBQ4YG4s34fqbujclxZTHxYWVVSNAiEAn3v5Xmct7fkLpkjGexiHsy6D90r0K2LlUqpN/069y5s=KP10012331550BC9TS1320200818004100ZPB1100000000000008BDD0D63BA2E438AFD9386DDA768D6328C27A02C0009DD6E8EDB52D366DADE4FA1397E6DD7B360F115726BF4563D6DC303107FB63EC5B05F0837F6BD290E5A678AC15162B624CD2669092F9B8993FC492F1BA535EF9E1E55182A4DBE73DABC30945D96B329C04FCB325376D188112E16C8AD1AF5E3F2F234103C785B92FD24AD7E816121542513294E0A561F00BE2B53
	)
	string(CONCAT tr31_tool_test36_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 1840 bytes[\r\n]"
		"Key usage: \\[S0\\] Asymmetric Key Pair for Digital Signature[\r\n]"
		"Key algorithm: \\[E\\] Elliptic Curve[\r\n]"
		"Key mode of use: \\[S\\] Signature Only[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Optional blocks \\[4\\]:[\r\n]"
		"\t\\[CT\\] Public Key Certificate: \"0002F0MIICLjCCAd.*K2LlUqpN/069y5s=\" \\(Certificate chain\\)[\r\n]"
		"\t\\[KP\\] Key Check Value \\(KCV\\) of KBPK: 2331550BC9 \\(CMAC based KCV\\)[\r\n]"
		"\t\\[TS\\] Time Stamp: \"20200818004100Z\" \\(.*\\)[\r\n]"
		"\t\\[PB\\] Padding Block: \"0000000000000\"[\r\n]"
		"Key length: 121[\r\n]"
		"Key value: 307702010104202D493257A45B34C11B6526A03DB4D8AE16EE87A0C16BDF1BE23C2DD8B164A2D3A00A06082A8648CE3D030107A14403420004423F48BAC7E9F21303DCBA6370677AEA13FF9F841D27A9671098792B2D1A5976B15BE64836565926CDEA587915BFF36A0AC38E343F819B5620E4DFDB02F5BD21[\r\n]"
	)
	set_tests_properties(tr31_tool_test36
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test36_regex}
	)

	add_test(NAME tr31_tool_test37
		COMMAND tr31-tool --kbpk FA36E44278DB3AB5F298F9F7DA8F1F88 --export 308204A40201000282010100D6C460FB012E8DEDF2D78574B451E9BF1C6963F7F2AE5741D2745CB410E7E00BE405FC89B832C6E7DBEA9065D577C27C610770107649AD5ABEEC1F2B8ED33A3CCA321FFED00196400BBB70EF8D0FE1C7DE9F522BCAE6D956F895F50529FA01D3A1E9597EBD6B7C8B2E106873A3A521C18EF49244BCEFFAD81C255AB616448CC9AF50811DA738343D9DBE1DE83A0F7E4E43BD08FA7A7E06C1107FBD59AFE77688AA95184779D25F2D6E7E16DC0E2B3B250444F4DA002C12F6BBDD17B9E5993A92239C9B37A9669B7EDAEF587BEE7E8CE7869AC362A4258595A3ED6CB544F2155B2FFD806CF5ACA56C11D0398AA9FD4DAD0744F1339F10606CF2696BCDA9B17339020301000102820101009A6193ED1ACE624BF7D2A1266130B8BC1E2A4C284214BCB89E15F345A519695E62CD42D9A4C52B62241D9B2AF8A61BF1D8B5C602AF650AEE3E6BF184182912A5FC1AC8111D68E69EA75058407AC03DE6B4CB060060DC4CC34DF24DAD269D868EA0C6E3044E1963EF906F4F06414E44D3A4757E67570192E9A261DFB12094AA36476582272EDAB0F56F816D9FA69580B3AB053237135BD1DDDB42AF77E11E1629F5A11B22C5E3E2DB3E8767A90B94414898DCCBB47EFE619F0620AC29C389FB464CE9C5E243963E13B6DA38EFAE1330AEFB54C0D5E2E59B6D7FBE4B3A22EE483F74E74D4A4A25978C65F4AC5829C34260930C85ECEA1FB24DB52A438D4EB2CF6102818100ED003DEEB8013AF4E4EBE172FE474B23FE20128805840C2D277E3E308D5B44527F1ED33C07AF350D5B22B27E082CD101DD2DE54DC8DF8F91D4BA5768EB9EC5F27DB5D358EA5E0DEE08A535677CB3F765789C9DAE56B7421B9E54525EF928AB2885BF098E837999AD0CA3CA6AC642EA9AD1331856B0CCEE5DF01FED1B2F63C3A502818100E7FBDA0A71C926DD51F037206D901A297554A39BBC9239794421F1F54D29766E50E016CA5701BDA79AEC543F5066B3730E053EA4B5872D25C29673CA8457B07390D11AF23F2247A1133FC2522B96CB02DA77D927FEE6661066058D1A4D85FE3C1D3418542F24B3982B4DDFB4192E512EDA1BAAAA59955B50945DD083E26E4D0502818002D4E0E88C3C3F87138119F574C2474C8BC9B84EF5B9E92754F4762BC05499D15E8170C6A3D4DD0E66CB5854972669ECDAC6A499B44FAF786F91366023888716E9979589D76AFE419CCAD4838302E76EC7ED1F1929221161211822CFCDAC45B73B39D81462CFBE1D4A2C5ECBBDC8A8E2E6A2F4A47C82464ACB06A69F8F86629D0281804E98A99AF84A2A7CB992255B3B43A3598083189B5F1C3B94B65CB9D95E373A04CE29DE0ED7C3A339F1E737F3EB8DA026CF0D3FD81618A25734C23CA0D48DD11E9666023728E4B857FE698FB0BF4BEBA41FD8931E55E2419A34B694C3E0981136D4BE1DB007F8EB5016FBDF5AE95D23EC37C13FE54F4CA70F79F4FEFC6FEEE6F1028181008321C2F1E8914C0AE09C418ACFEBB8A7B86A1E7144182F5145FBA90AF104DE3BC7604D86A831AC2F38DA356C99BC60BEA80E26EC8B7FAF8BB84A8661EF564BDC65DA0519F5E3CE81FF491CC71DA0813960048B225E61C56684D3CE01AE28A212C9ACCE946E2AAB80ADD51B00093029C5D52E9AF6C8A3EB861641B00E23636A68 --export-header D0000S0RS00N0000 --export-opt-block-CT-X509 MIIDszCCApugAwIBAgIIKpD5FKMfCZEwDQYJKoZIhvcNAQELBQAwLTEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxEjAQBgNVBAMMCVNhbXBsZSBDQTAeFw0yMDA4MTUwMjE0MTBaFw0yMTA4MTUwMjE0MTBaME8xFzAVBgNVBAoMDkFscGhhIE1lcmNoYW50MR8wHQYDVQQLDBZUTFMgQ2xpZW50IENlcnRpZmljYXRlMRMwEQYDVQQDDAoxMjM0NTY3ODkwMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA1sRg+wEuje3y14V0tFHpvxxpY/fyrldB0nRctBDn4AvkBfyJuDLG59vqkGXVd8J8YQdwEHZJrVq+7B8rjtM6PMoyH/7QAZZAC7tw740P4cfen1IryubZVviV9QUp+gHToelZfr1rfIsuEGhzo6UhwY70kkS87/rYHCVathZEjMmvUIEdpzg0PZ2+Heg6D35OQ70I+np+BsEQf71Zr+d2iKqVGEd50l8tbn4W3A4rOyUERPTaACwS9rvdF7nlmTqSI5ybN6lmm37a71h77n6M54aaw2KkJYWVo+1stUTyFVsv/YBs9aylbBHQOYqp/U2tB0TxM58QYGzyaWvNqbFzOQIDAQABo4G0MIGxMAkGA1UdEwQCMAAwDgYDVR0PAQH/BAQDAgeAMBMGA1UdJQQMMAoGCCsGAQUFBwMCMB0GA1UdDgQWBBR837QRAGx5uL9xDnRjr9L9WSBSlzAfBgNVHSMEGDAWgBSlXhVYy9bic9OLnRsxsFgKQQbLmTA/BgNVHR8EODA2MDSgMqAwhi5odHRwOi8vY3JsLmFscGhhLW1lcmNoYW50LmV4YW1wbGUvU2FtcGxlQ0EuY3JsMA0GCSqGSIb3DQEBCwUAA4IBAQCH6JusIBSkRDqzAohaSoJVAEwQGMdcUSQWDfMyJjZqkOep1kT8Sl7LolFmmmVRJdkTWZe4PxBfQUc/eIql9BIx90506B+j9aoVA7212OExAid78GgqKA6JoalhYQKRta9ixY8iolydTYyEYpegA1jFZavMQma4ZGwX/bDJWr4+cJYxJXWaf67g4AMqHaWC8J60MVjrrBe9BZ0ZstuIlNkktQUOZanqxqsrFeqz02ibwTwNHtaHQCztB4KgdTkrTNahkqeq6xjafDoTllNo1EddajnbA/cVzF9ZCNigDtg5chXHWIQbgEK7HmU3sY3/wd2Bh1KdF3+vpN+5iZMRNv7Z --export-opt-block-KP --export-opt-block-TS 20200818221218Z
	)
	set_tests_properties(tr31_tool_test37
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D3776S0RS00N0400CT0002050000MIIDszCCApugAwIBAgIIKpD5FKMfCZEwDQYJKoZIhvcNAQELBQAwLTEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxEjAQBgNVBAMMCVNhbXBsZSBDQTAeFw0yMDA4MTUwMjE0MTBaFw0yMTA4MTUwMjE0MTBaME8xFzAVBgNVBAoMDkFscGhhIE1lcmNoYW50MR8wHQYDVQQLDBZUTFMgQ2xpZW50IENlcnRpZmljYXRlMRMwEQYDVQQDDAoxMjM0NTY3ODkwMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA1sRg\\+wEuje3y14V0tFHpvxxpY/fyrldB0nRctBDn4AvkBfyJuDLG59vqkGXVd8J8YQdwEHZJrVq\\+7B8rjtM6PMoyH/7QAZZAC7tw740P4cfen1IryubZVviV9QUp\\+gHToelZfr1rfIsuEGhzo6UhwY70kkS87/rYHCVathZEjMmvUIEdpzg0PZ2\\+Heg6D35OQ70I\\+np\\+BsEQf71Zr\\+d2iKqVGEd50l8tbn4W3A4rOyUERPTaACwS9rvdF7nlmTqSI5ybN6lmm37a71h77n6M54aaw2KkJYWVo\\+1stUTyFVsv/YBs9aylbBHQOYqp/U2tB0TxM58QYGzyaWvNqbFzOQIDAQABo4G0MIGxMAkGA1UdEwQCMAAwDgYDVR0PAQH/BAQDAgeAMBMGA1UdJQQMMAoGCCsGAQUFBwMCMB0GA1UdDgQWBBR837QRAGx5uL9xDnRjr9L9WSBSlzAfBgNVHSMEGDAWgBSlXhVYy9bic9OLnRsxsFgKQQbLmTA/BgNVHR8EODA2MDSgMqAwhi5odHRwOi8vY3JsLmFscGhhLW1lcmNoYW50LmV4YW1wbGUvU2FtcGxlQ0EuY3JsMA0GCSqGSIb3DQEBCwUAA4IBAQCH6JusIBSkRDqzAohaSoJVAEwQGMdcUSQWDfMyJjZqkOep1kT8Sl7LolFmmmVRJdkTWZe4PxBfQUc/eIql9BIx90506B\\+j9aoVA7212OExAid78GgqKA6JoalhYQKRta9ixY8iolydTYyEYpegA1jFZavMQma4ZGwX/bDJWr4\\+cJYxJXWaf67g4AMqHaWC8J60MVjrrBe9BZ0ZstuIlNkktQUOZanqxqsrFeqz02ibwTwNHtaHQCztB4KgdTkrTNahkqeq6xjafDoTllNo1EddajnbA/cVzF9ZCNigDtg5chXHWIQbgEK7HmU3sY3/wd2Bh1KdF3\\+vpN\\+5iZMRNv7ZKP1001D77F007724TS1320200818221218ZPB0D"
	)

	add_test(NAME tr31_tool_test38
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --export 307702010104202D493257A45B34C11B6526A03DB4D8AE16EE87A0C16BDF1BE23C2DD8B164A2D3A00A06082A8648CE3D030107A14403420004423F48BAC7E9F21303DCBA6370677AEA13FF9F841D27A9671098792B2D1A5976B15BE64836565926CDEA587915BFF36A0AC38E343F819B5620E4DFDB02F5BD21 --export-header D0000S0ES00N0000 --export-opt-block-CT-X509 MIICLjCCAdSgAwIBAgIIGDrdWBxuNpAwCgYIKoZIzj0EAwIwMTEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxFjAUBgNVBAMMDVNhbXBsZSBFQ0MgQ0EwHhcNMjAwODE1MDIxMDEwWhcNMjEwODE1MDIxMDEwWjBPMRcwFQYDVQQKDA5BbHBoYSBNZXJjaGFudDEfMB0GA1UECwwWVExTIENsaWVudCBDZXJ0aWZpY2F0ZTETMBEGA1UEAwwKMTIzNDU2Nzg5MDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABEI/SLrH6fITA9y6Y3BneuoT/5+EHSepZxCYeSstGll2sVvmSDZWWSbN6lh5Fb/zagrDjjQ/gZtWIOTf2wL1vSGjgbcwgbQwCQYDVR0TBAIwADAOBgNVHQ8BAf8EBAMCB4AwEwYDVR0lBAwwCgYIKwYBBQUHAwIwHQYDVR0OBBYEFHuvP526vFMywEoVoXZ5aXNfhnfeMB8GA1UdIwQYMBaAFI+ZFhOWF+oMtcfYwg15vH5WmWccMEIGA1UdHwQ7MDkwN6A1oDOGMWh0dHA6Ly9jcmwuYWxwaGEtbWVyY2hhbnQuZXhhbXBsZS9TYW1wbGVFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSAAwRQIhAPuWWvCTmOdvQzUjCUmTX7H4sX4Ebpw+CI+aOQLu1DqwAiA0eR4FdMtvXV4P6+WMz5B10oea5xtLTfSgoBDoTkvKYQ== --export-opt-block-CT-X509 MIICDjCCAbOgAwIBAgIIfnOsCbsxHjwwCgYIKoZIzj0EAwIwNjEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxGzAZBgNVBAMMElNhbXBsZSBSb290IEVDQyBDQTAeFw0yMDA4MTUwMjEwMDlaFw0zMDA4MTMwMjEwMDlaMDExFzAVBgNVBAoMDkFscGhhIE1lcmNoYW50MRYwFAYDVQQDDA1TYW1wbGUgRUNDIENBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEHCanM9n+Rji+3EROj+HlogmXMU1Fk1td7N3I/8rfFnre1GwWCUqXSePHxwQ9DRHCV3oht3OUU2kDfitfUIujA6OBrzCBrDASBgNVHRMBAf8ECDAGAQH/AgEAMA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUj5kWE5YX6gy1x9jCDXm8flaZZxwwHwYDVR0jBBgwFoAUvElIifFlt6oeUaopV9Y0lJtyPVQwRgYDVR0fBD8wPTA7oDmgN4Y1aHR0cDovL2NybC5hbHBoYS1tZXJjaGFudC5leGFtcGxlL1NhbXBsZVJvb3RFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSQAwRgIhALT8+DG+++KuqqUGyBQ4YG4s34fqbujclxZTHxYWVVSNAiEAn3v5Xmct7fkLpkjGexiHsy6D90r0K2LlUqpN/069y5s= --export-opt-block-KP --export-opt-block-TS 20200818004100Z
	)
	set_tests_properties(tr31_tool_test38
		PROPERTIES
			PASS_REGULAR_EXPRESSION "D1840S0ES00N0400CT000205CC020002F0MIICLjCCAdSgAwIBAgIIGDrdWBxuNpAwCgYIKoZIzj0EAwIwMTEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxFjAUBgNVBAMMDVNhbXBsZSBFQ0MgQ0EwHhcNMjAwODE1MDIxMDEwWhcNMjEwODE1MDIxMDEwWjBPMRcwFQYDVQQKDA5BbHBoYSBNZXJjaGFudDEfMB0GA1UECwwWVExTIENsaWVudCBDZXJ0aWZpY2F0ZTETMBEGA1UEAwwKMTIzNDU2Nzg5MDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABEI/SLrH6fITA9y6Y3BneuoT/5\\+EHSepZxCYeSstGll2sVvmSDZWWSbN6lh5Fb/zagrDjjQ/gZtWIOTf2wL1vSGjgbcwgbQwCQYDVR0TBAIwADAOBgNVHQ8BAf8EBAMCB4AwEwYDVR0lBAwwCgYIKwYBBQUHAwIwHQYDVR0OBBYEFHuvP526vFMywEoVoXZ5aXNfhnfeMB8GA1UdIwQYMBaAFI\\+ZFhOWF\\+oMtcfYwg15vH5WmWccMEIGA1UdHwQ7MDkwN6A1oDOGMWh0dHA6Ly9jcmwuYWxwaGEtbWVyY2hhbnQuZXhhbXBsZS9TYW1wbGVFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSAAwRQIhAPuWWvCTmOdvQzUjCUmTX7H4sX4Ebpw\\+CI\\+aOQLu1DqwAiA0eR4FdMtvXV4P6\\+WMz5B10oea5xtLTfSgoBDoTkvKYQ==0002C4MIICDjCCAbOgAwIBAgIIfnOsCbsxHjwwCgYIKoZIzj0EAwIwNjEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxGzAZBgNVBAMMElNhbXBsZSBSb290IEVDQyBDQTAeFw0yMDA4MTUwMjEwMDlaFw0zMDA4MTMwMjEwMDlaMDExFzAVBgNVBAoMDkFscGhhIE1lcmNoYW50MRYwFAYDVQQDDA1TYW1wbGUgRUNDIENBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEHCanM9n\\+Rji\\+3EROj\\+HlogmXMU1Fk1td7N3I/8rfFnre1GwWCUqXSePHxwQ9DRHCV3oht3OUU2kDfitfUIujA6OBrzCBrDASBgNVHRMBAf8ECDAGAQH/AgEAMA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUj5kWE5YX6gy1x9jCDXm8flaZZxwwHwYDVR0jBBgwFoAUvElIifFlt6oeUaopV9Y0lJtyPVQwRgYDVR0fBD8wPTA7oDmgN4Y1aHR0cDovL2NybC5hbHBoYS1tZXJjaGFudC5leGFtcGxlL1NhbXBsZVJvb3RFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSQAwRgIhALT8\\+DG\\+\\+\\+KuqqUGyBQ4YG4s34fqbujclxZTHxYWVVSNAiEAn3v5Xmct7fkLpkjGexiHsy6D90r0K2LlUqpN/069y5s=KP10012331550BC9TS1320200818004100ZPB11"
	)

	add_test(NAME tr31_tool_test39
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --export 307702010104202D493257A45B34C11B6526A03DB4D8AE16EE87A0C16BDF1BE23C2DD8B164A2D3A00A06082A8648CE3D030107A14403420004423F48BAC7E9F21303DCBA6370677AEA13FF9F841D27A9671098792B2D1A5976B15BE64836565926CDEA587915BFF36A0AC38E343F819B5620E4DFDB02F5BD21 --export-header D0000S0ES00N0000 --export-opt-block-CT-X509 MIICLjCCAdSgAwIBAgIIGDrdWBxuNpAwCgYIKoZIzj0EAwIwMTEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxFjAUBgNVBAMMDVNhbXBsZSBFQ0MgQ0EwHhcNMjAwODE1MDIxMDEwWhcNMjEwODE1MDIxMDEwWjBPMRcwFQYDVQQKDA5BbHBoYSBNZXJjaGFudDEfMB0GA1UECwwWVExTIENsaWVudCBDZXJ0aWZpY2F0ZTETMBEGA1UEAwwKMTIzNDU2Nzg5MDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABEI/SLrH6fITA9y6Y3BneuoT/5+EHSepZxCYeSstGll2sVvmSDZWWSbN6lh5Fb/zagrDjjQ/gZtWIOTf2wL1vSGjgbcwgbQwCQYDVR0TBAIwADAOBgNVHQ8BAf8EBAMCB4AwEwYDVR0lBAwwCgYIKwYBBQUHAwIwHQYDVR0OBBYEFHuvP526vFMywEoVoXZ5aXNfhnfeMB8GA1UdIwQYMBaAFI+ZFhOWF+oMtcfYwg15vH5WmWccMEIGA1UdHwQ7MDkwN6A1oDOGMWh0dHA6Ly9jcmwuYWxwaGEtbWVyY2hhbnQuZXhhbXBsZS9TYW1wbGVFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSAAwRQIhAPuWWvCTmOdvQzUjCUmTX7H4sX4Ebpw+CI+aOQLu1DqwAiA0eR4FdMtvXV4P6+WMz5B10oea5xtLTfSgoBDoTkvKYQ== --export-opt-block-CT-X509 MIICDjCCAbOgAwIBAgIIfnOsCbsxHjwwCgYIKoZIzj0EAwIwNjEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxGzAZBgNVBAMMElNhbXBsZSBSb290IEVDQyBDQTAeFw0yMDA4MTUwMjEwMDlaFw0zMDA4MTMwMjEwMDlaMDExFzAVBgNVBAoMDkFscGhhIE1lcmNoYW50MRYwFAYDVQQDDA1TYW1wbGUgRUNDIENBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEHCanM9n+Rji+3EROj+HlogmXMU1Fk1td7N3I/8rfFnre1GwWCUqXSePHxwQ9DRHCV3oht3OUU2kDfitfUIujA6OBrzCBrDASBgNVHRMBAf8ECDAGAQH/AgEAMA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUj5kWE5YX6gy1x9jCDXm8flaZZxwwHwYDVR0jBBgwFoAUvElIifFlt6oeUaopV9Y0lJtyPVQwRgYDVR0fBD8wPTA7oDmgN4Y1aHR0cDovL2NybC5hbHBoYS1tZXJjaGFudC5leGFtcGxlL1NhbXBsZVJvb3RFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSQAwRgIhALT8+DG+++KuqqUGyBQ4YG4s34fqbujclxZTHxYWVVSNAiEAn3v5Xmct7fkLpkjGexiHsy6D90r0K2LlUqpN/069y5s= --export-opt-block-CT-EMV asdf --export-opt-block-KP --export-opt-block-TS 20200818004100Z
	)
	set_tests_properties(tr31_tool_test39
		PROPERTIES
			PASS_REGULAR_EXPRESSION "D1840S0ES00N0400CT000205D6020002F0MIICLjCCAdSgAwIBAgIIGDrdWBxuNpAwCgYIKoZIzj0EAwIwMTEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxFjAUBgNVBAMMDVNhbXBsZSBFQ0MgQ0EwHhcNMjAwODE1MDIxMDEwWhcNMjEwODE1MDIxMDEwWjBPMRcwFQYDVQQKDA5BbHBoYSBNZXJjaGFudDEfMB0GA1UECwwWVExTIENsaWVudCBDZXJ0aWZpY2F0ZTETMBEGA1UEAwwKMTIzNDU2Nzg5MDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABEI/SLrH6fITA9y6Y3BneuoT/5\\+EHSepZxCYeSstGll2sVvmSDZWWSbN6lh5Fb/zagrDjjQ/gZtWIOTf2wL1vSGjgbcwgbQwCQYDVR0TBAIwADAOBgNVHQ8BAf8EBAMCB4AwEwYDVR0lBAwwCgYIKwYBBQUHAwIwHQYDVR0OBBYEFHuvP526vFMywEoVoXZ5aXNfhnfeMB8GA1UdIwQYMBaAFI\\+ZFhOWF\\+oMtcfYwg15vH5WmWccMEIGA1UdHwQ7MDkwN6A1oDOGMWh0dHA6Ly9jcmwuYWxwaGEtbWVyY2hhbnQuZXhhbXBsZS9TYW1wbGVFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSAAwRQIhAPuWWvCTmOdvQzUjCUmTX7H4sX4Ebpw\\+CI\\+aOQLu1DqwAiA0eR4FdMtvXV4P6\\+WMz5B10oea5xtLTfSgoBDoTkvKYQ==0002C4MIICDjCCAbOgAwIBAgIIfnOsCbsxHjwwCgYIKoZIzj0EAwIwNjEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxGzAZBgNVBAMMElNhbXBsZSBSb290IEVDQyBDQTAeFw0yMDA4MTUwMjEwMDlaFw0zMDA4MTMwMjEwMDlaMDExFzAVBgNVBAoMDkFscGhhIE1lcmNoYW50MRYwFAYDVQQDDA1TYW1wbGUgRUNDIENBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEHCanM9n\\+Rji\\+3EROj\\+HlogmXMU1Fk1td7N3I/8rfFnre1GwWCUqXSePHxwQ9DRHCV3oht3OUU2kDfitfUIujA6OBrzCBrDASBgNVHRMBAf8ECDAGAQH/AgEAMA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUj5kWE5YX6gy1x9jCDXm8flaZZxwwHwYDVR0jBBgwFoAUvElIifFlt6oeUaopV9Y0lJtyPVQwRgYDVR0fBD8wPTA7oDmgN4Y1aHR0cDovL2NybC5hbHBoYS1tZXJjaGFudC5leGFtcGxlL1NhbXBsZVJvb3RFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSQAwRgIhALT8\\+DG\\+\\+\\+KuqqUGyBQ4YG4s34fqbujclxZTHxYWVVSNAiEAn3v5Xmct7fkLpkjGexiHsy6D90r0K2LlUqpN/069y5s=010004asdfKP10012331550BC9TS1320200818004100ZPB07"
	)

	add_test(NAME tr31_tool_test40
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --export 307702010104202D493257A45B34C11B6526A03DB4D8AE16EE87A0C16BDF1BE23C2DD8B164A2D3A00A06082A8648CE3D030107A14403420004423F48BAC7E9F21303DCBA6370677AEA13FF9F841D27A9671098792B2D1A5976B15BE64836565926CDEA587915BFF36A0AC38E343F819B5620E4DFDB02F5BD21 --export-header D0000S0ES00N0000 --export-opt-block-CT-X509 MIICLjCCAdSgAwIBAgIIGDrdWBxuNpAwCgYIKoZIzj0EAwIwMTEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxFjAUBgNVBAMMDVNhbXBsZSBFQ0MgQ0EwHhcNMjAwODE1MDIxMDEwWhcNMjEwODE1MDIxMDEwWjBPMRcwFQYDVQQKDA5BbHBoYSBNZXJjaGFudDEfMB0GA1UECwwWVExTIENsaWVudCBDZXJ0aWZpY2F0ZTETMBEGA1UEAwwKMTIzNDU2Nzg5MDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABEI/SLrH6fITA9y6Y3BneuoT/5+EHSepZxCYeSstGll2sVvmSDZWWSbN6lh5Fb/zagrDjjQ/gZtWIOTf2wL1vSGjgbcwgbQwCQYDVR0TBAIwADAOBgNVHQ8BAf8EBAMCB4AwEwYDVR0lBAwwCgYIKwYBBQUHAwIwHQYDVR0OBBYEFHuvP526vFMywEoVoXZ5aXNfhnfeMB8GA1UdIwQYMBaAFI+ZFhOWF+oMtcfYwg15vH5WmWccMEIGA1UdHwQ7MDkwN6A1oDOGMWh0dHA6Ly9jcmwuYWxwaGEtbWVyY2hhbnQuZXhhbXBsZS9TYW1wbGVFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSAAwRQIhAPuWWvCTmOdvQzUjCUmTX7H4sX4Ebpw+CI+aOQLu1DqwAiA0eR4FdMtvXV4P6+WMz5B10oea5xtLTfSgoBDoTkvKYQ== --export-opt-block-CT-X509 MIICDjCCAbOgAwIBAgIIfnOsCbsxHjwwCgYIKoZIzj0EAwIwNjEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxGzAZBgNVBAMMElNhbXBsZSBSb290IEVDQyBDQTAeFw0yMDA4MTUwMjEwMDlaFw0zMDA4MTMwMjEwMDlaMDExFzAVBgNVBAoMDkFscGhhIE1lcmNoYW50MRYwFAYDVQQDDA1TYW1wbGUgRUNDIENBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEHCanM9n+Rji+3EROj+HlogmXMU1Fk1td7N3I/8rfFnre1GwWCUqXSePHxwQ9DRHCV3oht3OUU2kDfitfUIujA6OBrzCBrDASBgNVHRMBAf8ECDAGAQH/AgEAMA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUj5kWE5YX6gy1x9jCDXm8flaZZxwwHwYDVR0jBBgwFoAUvElIifFlt6oeUaopV9Y0lJtyPVQwRgYDVR0fBD8wPTA7oDmgN4Y1aHR0cDovL2NybC5hbHBoYS1tZXJjaGFudC5leGFtcGxlL1NhbXBsZVJvb3RFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSQAwRgIhALT8+DG+++KuqqUGyBQ4YG4s34fqbujclxZTHxYWVVSNAiEAn3v5Xmct7fkLpkjGexiHsy6D90r0K2LlUqpN/069y5s= --export-opt-block-AL Static --export-opt-block-KP --export-opt-block-TS 20200818004100Z
	)
	set_tests_properties(tr31_tool_test40
		PROPERTIES
			PASS_REGULAR_EXPRESSION "D1840S0ES00N0500AL080101CT000205CC020002F0MIICLjCCAdSgAwIBAgIIGDrdWBxuNpAwCgYIKoZIzj0EAwIwMTEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxFjAUBgNVBAMMDVNhbXBsZSBFQ0MgQ0EwHhcNMjAwODE1MDIxMDEwWhcNMjEwODE1MDIxMDEwWjBPMRcwFQYDVQQKDA5BbHBoYSBNZXJjaGFudDEfMB0GA1UECwwWVExTIENsaWVudCBDZXJ0aWZpY2F0ZTETMBEGA1UEAwwKMTIzNDU2Nzg5MDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABEI/SLrH6fITA9y6Y3BneuoT/5\\+EHSepZxCYeSstGll2sVvmSDZWWSbN6lh5Fb/zagrDjjQ/gZtWIOTf2wL1vSGjgbcwgbQwCQYDVR0TBAIwADAOBgNVHQ8BAf8EBAMCB4AwEwYDVR0lBAwwCgYIKwYBBQUHAwIwHQYDVR0OBBYEFHuvP526vFMywEoVoXZ5aXNfhnfeMB8GA1UdIwQYMBaAFI\\+ZFhOWF\\+oMtcfYwg15vH5WmWccMEIGA1UdHwQ7MDkwN6A1oDOGMWh0dHA6Ly9jcmwuYWxwaGEtbWVyY2hhbnQuZXhhbXBsZS9TYW1wbGVFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSAAwRQIhAPuWWvCTmOdvQzUjCUmTX7H4sX4Ebpw\\+CI\\+aOQLu1DqwAiA0eR4FdMtvXV4P6\\+WMz5B10oea5xtLTfSgoBDoTkvKYQ==0002C4MIICDjCCAbOgAwIBAgIIfnOsCbsxHjwwCgYIKoZIzj0EAwIwNjEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxGzAZBgNVBAMMElNhbXBsZSBSb290IEVDQyBDQTAeFw0yMDA4MTUwMjEwMDlaFw0zMDA4MTMwMjEwMDlaMDExFzAVBgNVBAoMDkFscGhhIE1lcmNoYW50MRYwFAYDVQQDDA1TYW1wbGUgRUNDIENBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEHCanM9n\\+Rji\\+3EROj\\+HlogmXMU1Fk1td7N3I/8rfFnre1GwWCUqXSePHxwQ9DRHCV3oht3OUU2kDfitfUIujA6OBrzCBrDASBgNVHRMBAf8ECDAGAQH/AgEAMA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUj5kWE5YX6gy1x9jCDXm8flaZZxwwHwYDVR0jBBgwFoAUvElIifFlt6oeUaopV9Y0lJtyPVQwRgYDVR0fBD8wPTA7oDmgN4Y1aHR0cDovL2NybC5hbHBoYS1tZXJjaGFudC5leGFtcGxlL1NhbXBsZVJvb3RFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSQAwRgIhALT8\\+DG\\+\\+\\+KuqqUGyBQ4YG4s34fqbujclxZTHxYWVVSNAiEAn3v5Xmct7fkLpkjGexiHsy6D90r0K2LlUqpN/069y5s=KP10012331550BC9TS1320200818004100ZPB09"
	)

	add_test(NAME tr31_tool_test41
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --import D1840S0ES00N0500AL080101CT000205CC020002F0MIICLjCCAdSgAwIBAgIIGDrdWBxuNpAwCgYIKoZIzj0EAwIwMTEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxFjAUBgNVBAMMDVNhbXBsZSBFQ0MgQ0EwHhcNMjAwODE1MDIxMDEwWhcNMjEwODE1MDIxMDEwWjBPMRcwFQYDVQQKDA5BbHBoYSBNZXJjaGFudDEfMB0GA1UECwwWVExTIENsaWVudCBDZXJ0aWZpY2F0ZTETMBEGA1UEAwwKMTIzNDU2Nzg5MDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABEI/SLrH6fITA9y6Y3BneuoT/5+EHSepZxCYeSstGll2sVvmSDZWWSbN6lh5Fb/zagrDjjQ/gZtWIOTf2wL1vSGjgbcwgbQwCQYDVR0TBAIwADAOBgNVHQ8BAf8EBAMCB4AwEwYDVR0lBAwwCgYIKwYBBQUHAwIwHQYDVR0OBBYEFHuvP526vFMywEoVoXZ5aXNfhnfeMB8GA1UdIwQYMBaAFI+ZFhOWF+oMtcfYwg15vH5WmWccMEIGA1UdHwQ7MDkwN6A1oDOGMWh0dHA6Ly9jcmwuYWxwaGEtbWVyY2hhbnQuZXhhbXBsZS9TYW1wbGVFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSAAwRQIhAPuWWvCTmOdvQzUjCUmTX7H4sX4Ebpw+CI+aOQLu1DqwAiA0eR4FdMtvXV4P6+WMz5B10oea5xtLTfSgoBDoTkvKYQ==0002C4MIICDjCCAbOgAwIBAgIIfnOsCbsxHjwwCgYIKoZIzj0EAwIwNjEXMBUGA1UECgwOQWxwaGEgTWVyY2hhbnQxGzAZBgNVBAMMElNhbXBsZSBSb290IEVDQyBDQTAeFw0yMDA4MTUwMjEwMDlaFw0zMDA4MTMwMjEwMDlaMDExFzAVBgNVBAoMDkFscGhhIE1lcmNoYW50MRYwFAYDVQQDDA1TYW1wbGUgRUNDIENBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEHCanM9n+Rji+3EROj+HlogmXMU1Fk1td7N3I/8rfFnre1GwWCUqXSePHxwQ9DRHCV3oht3OUU2kDfitfUIujA6OBrzCBrDASBgNVHRMBAf8ECDAGAQH/AgEAMA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUj5kWE5YX6gy1x9jCDXm8flaZZxwwHwYDVR0jBBgwFoAUvElIifFlt6oeUaopV9Y0lJtyPVQwRgYDVR0fBD8wPTA7oDmgN4Y1aHR0cDovL2NybC5hbHBoYS1tZXJjaGFudC5leGFtcGxlL1NhbXBsZVJvb3RFQ0NDQS5jcmwwCgYIKoZIzj0EAwIDSQAwRgIhALT8+DG+++KuqqUGyBQ4YG4s34fqbujclxZTHxYWVVSNAiEAn3v5Xmct7fkLpkjGexiHsy6D90r0K2LlUqpN/069y5s=KP10012331550BC9TS1320200818004100ZPB095p7XA1AFDA0F934B03EF4032BAA0F7032B9987A8F992BB26D15855559C962B31B47D52FEB75D3D028BCB59547F12B78408D40E5BC9B9965E37C5263E9A87FEB6F7E57BD64BF82911DA27420B088A3B1E718A20A1ECBD28D2194F8B037AF081C561E4B5680C8E53D3047792B9D24B36E8D608C30EF91F40E496B463D308A4E0A85E2630276565CF0A6D6C0714A7D946BE95666
	)
	string(CONCAT tr31_tool_test41_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 1840 bytes[\r\n]"
		"Key usage: \\[S0\\] Asymmetric Key Pair for Digital Signature[\r\n]"
		"Key algorithm: \\[E\\] Elliptic Curve[\r\n]"
		"Key mode of use: \\[S\\] Signature Only[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Optional blocks \\[5\\]:[\r\n]"
		"\t\\[AL\\] Asymmetric Key Life \\(AKL\\): v1, 01 \\(Static\\)[\r\n]"
		"\t\\[CT\\] Public Key Certificate: \"0002F0MIICLjCCAd.*K2LlUqpN/069y5s=\" \\(Certificate chain\\)[\r\n]"
		"\t\\[KP\\] Key Check Value \\(KCV\\) of KBPK: 2331550BC9 \\(CMAC based KCV\\)[\r\n]"
		"\t\\[TS\\] Time Stamp: \"20200818004100Z\" \\(.*\\)[\r\n]"
		"\t\\[PB\\] Padding Block: \"5p7XA\"[\r\n]"
		"Key length: 121[\r\n]"
		"Key value: 307702010104202D493257A45B34C11B6526A03DB4D8AE16EE87A0C16BDF1BE23C2DD8B164A2D3A00A06082A8648CE3D030107A14403420004423F48BAC7E9F21303DCBA6370677AEA13FF9F841D27A9671098792B2D1A5976B15BE64836565926CDEA587915BFF36A0AC38E343F819B5620E4DFDB02F5BD21[\r\n]"
	)
	set_tests_properties(tr31_tool_test41
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test41_regex}
	)

	# test export header containing optional blocks and padding, but not adding additional optional blocks
	add_test(NAME tr31_tool_test42
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --export 3F419E1CB7079442AA37474C2EFBF8B8 --export-header D1234M7HG00N0200HM0621PB0A5V5E8F
	)
	set_tests_properties(tr31_tool_test42
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0128M7HG00N0200HM0621PB0A5V5E8F" # note that optional block padding content is intentionally preserved
	)

	# test export header containing optional blocks and padding and also adding additional optional blocks
	add_test(NAME tr31_tool_test43
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --export 3F419E1CB7079442AA37474C2EFBF8B8 --export-header D1234M7HG00N0200HM0621PB0A5V5E8F --export-opt-block-LB "MyKey"
	)
	set_tests_properties(tr31_tool_test43
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0144M7HG00N0300HM0621LB09MyKeyPB11"
	)

	# test export header containing optional blocks but insufficient padding
	add_test(NAME tr31_tool_test44
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --export 3F419E1CB7079442AA37474C2EFBF8B8 --export-header D1234M7HG00N0100HM0621
	)
	set_tests_properties(tr31_tool_test44
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0128M7HG00N0200HM0621PB0A"
	)

	# test --import-no-strict-validation with non-standard mode of use
	add_test(NAME tr31_tool_test45
		COMMAND tr31-tool --import "B0128B1T900N0300KC0C000169E3KP0C00ECAD62KS18FFFF00A0200001E0000030EFDCB3A740B46D5BA7896587151A41E3D746CF063D0FC2D3FA45B851D06A43" --kbpk AB2E09DB3EF0BA71E0CE6CD755C23A3B --import-no-strict-validation
	)
	string(CONCAT tr31_tool_test45_regex
		"^Key block format version: B[\r\n]"
		"Key block length: 128 bytes[\r\n]"
		"Key usage: \\[B1\\] Initial DUKPT Key \\(IK/IPEK\\)[\r\n]"
		"Key algorithm: \\[T\\] TDES[\r\n]"
		"Key mode of use: \\[9\\] Unknown key mode of use value[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Optional blocks \\[3\\]:[\r\n]"
		"\t\\[KC\\] Key Check Value \\(KCV\\) of wrapped key: 0169E3 \\(Legacy KCV algorithm\\)[\r\n]"
		"\t\\[KP\\] Key Check Value \\(KCV\\) of KBPK: ECAD62 \\(Legacy KCV algorithm\\)[\r\n]"
		"\t\\[KS\\] Initial Key Serial Number \\(KSN\\): FFFF00A0200001E00000[\r\n]"
		"Key length: 16[\r\n]"
		"Key value: BF82DAC6A33DF92CE66E15B70E5DCEB6 \\(KCV: 0169E3\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test45
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test45_regex}
	)

	# test --import-no-strict-validation with non-standard optional block KS
	add_test(NAME tr31_tool_test46
		COMMAND tr31-tool --import "B0120B1TX00N0300KS10DE#GBIC#OPT1KC0C000169E3KP0C00ECAD627E13D36248CFF4CA9D2F880E2401B3727F96C2D92E5FDCA0D932A6DA80CAE8B9" --kbpk AB2E09DB3EF0BA71E0CE6CD755C23A3B --import-no-strict-validation
	)
	string(CONCAT tr31_tool_test46_regex
		"^Key block format version: B[\r\n]"
		"Key block length: 120 bytes[\r\n]"
		"Key usage: \\[B1\\] Initial DUKPT Key \\(IK/IPEK\\)[\r\n]"
		"Key algorithm: \\[T\\] TDES[\r\n]"
		"Key mode of use: \\[X\\] Key Derivation[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Optional blocks \\[3\\]:[\r\n]"
		"\t\\[KS\\] Initial Key Serial Number \\(KSN\\): DE#GBIC#OPT1[\r\n]"
		"\t\\[KC\\] Key Check Value \\(KCV\\) of wrapped key: 0169E3 \\(Legacy KCV algorithm\\)[\r\n]"
		"\t\\[KP\\] Key Check Value \\(KCV\\) of KBPK: ECAD62 \\(Legacy KCV algorithm\\)[\r\n]"
		"Key length: 16[\r\n]"
		"Key value: BF82DAC6A33DF92CE66E15B70E5DCEB6 \\(KCV: 0169E3\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test46
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test46_regex}
	)

	# test --export-opt-block with non-standard optional block KS
	add_test(NAME tr31_tool_test47
		COMMAND tr31-tool --kbpk 1D22BF32387C600AD97F9B97A51311AC --export E8BC63E5479455E26577F715D587FE68 --export-header "B0000B1TX00N0100LB0FProprietary" --export-opt-block "KS10DE#GBIC#OPT1"
	)
	set_tests_properties(tr31_tool_test47
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^B0136B1TX00N0300LB0FProprietaryKS10DE#GBIC#OPT1PB09"
	)

	# test --export-header with non-standard key usage
	add_test(NAME tr31_tool_test48
		COMMAND tr31-tool --kbpk 1D22BF32387C600AD97F9B97A51311AC --export E8BC63E5479455E26577F715D587FE68 --export-header "B000099TX00N0100LB0FProprietary"
	)
	set_tests_properties(tr31_tool_test48
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^B012099TX00N0200LB0FProprietaryPB09"
	)

	# test --import with non-zero key context
	add_test(NAME tr31_tool_test49
		COMMAND tr31-tool --import "B0096B1TX00N002042D72C7FF16FC1A9B1F9488AAAB27AEFDA96AA79D247071CD64F1CAA898006E8C03DFBF0EAC064D0" --kbpk AB2E09DB3EF0BA71E0CE6CD755C23A3B
	)
	string(CONCAT tr31_tool_test49_regex
		"^Key block format version: B[\r\n]"
		"Key block length: 96 bytes[\r\n]"
		"Key usage: \\[B1\\] Initial DUKPT Key \\(IK/IPEK\\)[\r\n]"
		"Key algorithm: \\[T\\] TDES[\r\n]"
		"Key mode of use: \\[X\\] Key Derivation[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[2\\] Key exchange context only[\r\n]"
		"Key length: 16[\r\n]"
		"Key value: BF82DAC6A33DF92CE66E15B70E5DCEB6 \\(KCV: 0169E3\\)[\r\n]"
	)
	set_tests_properties(tr31_tool_test49
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test49_regex}
	)

	# test --export with non-zero key context
	add_test(NAME tr31_tool_test50
		COMMAND tr31-tool --kbpk AB2E09DB3EF0BA71E0CE6CD755C23A3B --export BF82DAC6A33DF92CE66E15B70E5DCEB6 --export-header "B0000B1TX00N0020"
	)
	set_tests_properties(tr31_tool_test50
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^B0096B1TX00N0020[A-Z0-9]*[\r\n]"
	)

	# test HMAC key ISO 20038
	add_test(NAME tr31_tool_test51
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --export 3F419E1CB7079442AA37474C2EFBF8B8 --export-header D0000M7JG00N0000
	)
	set_tests_properties(tr31_tool_test51
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^D0112M7JG00N0000"
	)

	# test HMAC key ISO 20038
	add_test(NAME tr31_tool_test52
		COMMAND tr31-tool --kbpk 88E1AB2A2E3DD38C1FA039A536500CC8A87AB9D62DC92C01058FA79F44657DE6 --import D0112M7HG00N00000AD79E1883C16BCF2C980A087A348173A8F1326B9B1F6239738773BAA348FBFD0C0EC5AFD14A1A4BE2C4444215F2B823
	)
	string(CONCAT tr31_tool_test52_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 112 bytes[\r\n]"
		"Key usage: \\[M7\\] HMAC Key[\r\n]"
		"Key algorithm: \\[H\\] HMAC-SHA-1 \\(ISO 20038\\)[\r\n]"
		"Key mode of use: \\[G\\] MAC Generate Only[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Key length: 16[\r\n]"
		"Key value: 3F419E1CB7079442AA37474C2EFBF8B8[\r\n]"
	)
	set_tests_properties(tr31_tool_test52
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test52_regex}
	)

	# test IBM proprietary key block
	# NOTE: the input was hand crafted based on https://www.ibm.com/docs/en/zos/3.2.0?topic=ktf-x9143-tr-31-key-block-header-optional-block-data and https://www.ibm.com/docs/en/linux-on-systems?topic=data-tr-31-optional-block
	add_test(NAME tr31_tool_test53
		COMMAND tr31-tool --import D014410A100N0200101CIBMC01140123456789ABCDEFPB04000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000 --import-no-strict-validation
	)
	string(CONCAT tr31_tool_test53_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 144 bytes[\r\n]"
		"Key usage: \\[10\\] IBM[\r\n]"
		"Key algorithm: \\[A\\] AES[\r\n]"
		"Key mode of use: \\[1\\] IBM[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Optional blocks \\[2\\]:[\r\n]"
		"\t\\[10\\] IBM: IBMC01140123456789ABCDEF \\(Common Cryptographic Architecture \\(CCA\\) Control Vector \\(CV\\)\\)[\r\n]"
		"\t\\[PB\\] Padding Block: \"\"[\r\n]"
		"Key not decrypted[\r\n]"
	)
	set_tests_properties(tr31_tool_test53
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test53_regex}
	)

	# test non-IBM proprietary key block
	# NOTE: the input was hand crafted based on tr31_tool_test53 but excludes "IBMC"
	add_test(NAME tr31_tool_test54
		COMMAND tr31-tool --import D014410A100N0200101801140123456789ABCDEFPB08asdf000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000 --import-no-strict-validation
	)
	string(CONCAT tr31_tool_test54_regex
		"^Key block format version: D[\r\n]"
		"Key block length: 144 bytes[\r\n]"
		"Key usage: \\[10\\] Unknown key usage value[\r\n]"
		"Key algorithm: \\[A\\] AES[\r\n]"
		"Key mode of use: \\[1\\] Unknown key mode of use value[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Optional blocks \\[2\\]:[\r\n]"
		"\t\\[10\\] Unknown: 01140123456789ABCDEF[\r\n]"
		"\t\\[PB\\] Padding Block: \"asdf\"[\r\n]"
		"Key not decrypted[\r\n]"
	)
	set_tests_properties(tr31_tool_test54
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_tool_test54_regex}
	)
endif()

# Tests for the atomic key block tools
#
# All vectors below are derived from the key block and KBPK of tr31_tool_test1,
# so they can be cross-checked against tr31-tool or any other implementation:
#
#   KBPK (TDES2, 16 bytes)     : AB2E09DB3EF0BA71E0CE6CD755C23A3B
#   Key block (format version B, 128 bytes):
#     B0128B1TX00N0300KS18FFFF00A0200001E00000KC0C000169E3KP0C00ECAD62
#     6F9F1A826814AA066D86C8C18BD0E14033E1EBEC75BEDF586E6E325F3AA8C0E5
#   ASCII header (64 bytes)    : B0128B1TX00N0300KS18FFFF00A0200001E00000KC0C000169E3KP0C00ECAD62
#   Encrypted payload (24 bytes): 6F9F1A826814AA066D86C8C18BD0E14033E1EBEC75BEDF58
#   Authenticator (8 bytes)     : 6E6E325F3AA8C0E5
#   Wrapped key (16 bytes)      : BF82DAC6A33DF92CE66E15B70E5DCEB6
#
# Intermediate keys derived from the KBPK for format version B (TDES Key
# Derivation Binding Method); reproduce with:
#   tr31-kbpk --kbpk AB2E09DB3EF0BA71E0CE6CD755C23A3B
#     KBEK : 7F352D458B7B3069A5C79A2290B22FC7
#     KBAK : F2A8CE3BAAB27F1FB344A4AB288ADEDF
if(TARGET tr31-kcv AND BUILD_TESTING)
	# tr31-kcv: TDES legacy KCV of the wrapped key (matches KC optional block in tr31_tool_test1)
	add_test(NAME tr31_kcv_test1
		COMMAND tr31-kcv --key BF82DAC6A33DF92CE66E15B70E5DCEB6 --algorithm TDES
	)
	set_tests_properties(tr31_kcv_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^KCV: 0169E3[\r\n]"
	)

	# tr31-kcv: TDES CMAC KCV of the wrapped key BF82DAC6A33DF92CE66E15B70E5DCEB6
	add_test(NAME tr31_kcv_test2
		COMMAND tr31-kcv --key BF82DAC6A33DF92CE66E15B70E5DCEB6 --algorithm TDES --kcv-algorithm cmac
	)
	set_tests_properties(tr31_kcv_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^KCV: 697CE2E902[\r\n]"
	)

	# tr31-kcv: AES CMAC KCV of the wrapped key from tr31_tool_test15
	add_test(NAME tr31_kcv_test3
		COMMAND tr31-kcv --key 3F419E1CB7079442AA37474C2EFBF8B8 --algorithm AES
	)
	set_tests_properties(tr31_kcv_test3
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^KCV: 08793E25AB[\r\n]"
	)

	# tr31-kcv: list all KCV combinations when --algorithm is omitted
	add_test(NAME tr31_kcv_test4
		COMMAND tr31-kcv --key BF82DAC6A33DF92CE66E15B70E5DCEB6
	)
	string(CONCAT tr31_kcv_test4_regex
		"^Key: BF82DAC6A33DF92CE66E15B70E5DCEB6 \\(16 bytes\\)[\r\n]"
		"[\r\n]"
		"TDES legacy KCV: 0169E3[\r\n]"
		"TDES CMAC KCV: 697CE2E902[\r\n]"
		"AES CMAC KCV: 36EE13CB84[\r\n]"
	)
	set_tests_properties(tr31_kcv_test4
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_kcv_test4_regex}
	)

	# ASCII header (as hex) concatenated with the decrypted payload of the
	# tr31_tool_test1 key block; this is the data over which the authenticator
	# is computed. KBPK: AB2E09DB3EF0BA71E0CE6CD755C23A3B
	set(tr31_atomic_mac_data "42303132384231545830304E303330304B53313846464646303041303230303030314530303030304B43304330303031363945334B50304330304543414436320080BF82DAC6A33DF92CE66E15B70E5DCEB6E0F5A23E643C")

	# tr31-mac: generate version B authenticator (TDES CMAC)
	# KBAK F2A8CE3BAAB27F1FB344A4AB288ADEDF is derived from KBPK AB2E09DB3EF0BA71E0CE6CD755C23A3B (see tr31-kbpk)
	add_test(NAME tr31_mac_test1
		COMMAND tr31-mac --version-id B --kbak F2A8CE3BAAB27F1FB344A4AB288ADEDF --data ${tr31_atomic_mac_data}
	)
	set_tests_properties(tr31_mac_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^MAC: 6E6E325F3AA8C0E5[\r\n]"
	)

	# tr31-mac: verify version B authenticator
	# KBAK F2A8CE3BAAB27F1FB344A4AB288ADEDF is derived from KBPK AB2E09DB3EF0BA71E0CE6CD755C23A3B (see tr31-kbpk)
	add_test(NAME tr31_mac_test2
		COMMAND tr31-mac --version-id B --kbak F2A8CE3BAAB27F1FB344A4AB288ADEDF --data ${tr31_atomic_mac_data} --verify 6E6E325F3AA8C0E5
	)
	set_tests_properties(tr31_mac_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION "MAC verification successful"
	)

	# tr31-mac: verification failure for incorrect authenticator
	# KBAK F2A8CE3BAAB27F1FB344A4AB288ADEDF is derived from KBPK AB2E09DB3EF0BA71E0CE6CD755C23A3B (see tr31-kbpk)
	add_test(NAME tr31_mac_test3
		COMMAND tr31-mac --version-id B --kbak F2A8CE3BAAB27F1FB344A4AB288ADEDF --data ${tr31_atomic_mac_data} --verify 0000000000000000
	)
	set_tests_properties(tr31_mac_test3
		PROPERTIES
			WILL_FAIL TRUE
	)

	# tr31-crypt: decrypt version B payload (IV is the authenticator)
	# KBEK 7F352D458B7B3069A5C79A2290B22FC7 is derived from KBPK AB2E09DB3EF0BA71E0CE6CD755C23A3B (see tr31-kbpk)
	add_test(NAME tr31_crypt_test1
		COMMAND tr31-crypt --decrypt --version-id B --kbek 7F352D458B7B3069A5C79A2290B22FC7 --iv 6E6E325F3AA8C0E5 --data 6F9F1A826814AA066D86C8C18BD0E14033E1EBEC75BEDF58
	)
	set_tests_properties(tr31_crypt_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^0080BF82DAC6A33DF92CE66E15B70E5DCEB6E0F5A23E643C[\r\n]"
	)

	# tr31-crypt: encrypt version B payload (round-trip of tr31_crypt_test1)
	# KBEK 7F352D458B7B3069A5C79A2290B22FC7 is derived from KBPK AB2E09DB3EF0BA71E0CE6CD755C23A3B (see tr31-kbpk)
	add_test(NAME tr31_crypt_test2
		COMMAND tr31-crypt --encrypt --version-id B --kbek 7F352D458B7B3069A5C79A2290B22FC7 --iv 6E6E325F3AA8C0E5 --data 0080BF82DAC6A33DF92CE66E15B70E5DCEB6E0F5A23E643C
	)
	set_tests_properties(tr31_crypt_test2
		PROPERTIES
			PASS_REGULAR_EXPRESSION "^6F9F1A826814AA066D86C8C18BD0E14033E1EBEC75BEDF58[\r\n]"
	)

	# tr31-header: decode header and optional blocks (key block from tr31_tool_test1, KBPK AB2E09DB3EF0BA71E0CE6CD755C23A3B)
	add_test(NAME tr31_header_test1
		COMMAND tr31-header --header B0128B1TX00N0300KS18FFFF00A0200001E00000KC0C000169E3KP0C00ECAD626F9F1A826814AA066D86C8C18BD0E14033E1EBEC75BEDF586E6E325F3AA8C0E5
	)
	string(CONCAT tr31_header_test1_regex
		"^Key block format version: B[\r\n]"
		"Key usage: \\[B1\\] Initial DUKPT Key \\(IK/IPEK\\)[\r\n]"
		"Key algorithm: \\[T\\] TDES[\r\n]"
		"Key mode of use: \\[X\\] Key Derivation[\r\n]"
		"Key version: Unused[\r\n]"
		"Key exportability: \\[N\\] Not exportable[\r\n]"
		"Key context: \\[0\\] Determined by wrapping key[\r\n]"
		"Optional blocks \\[3\\]:[\r\n]"
		"\t\\[KS\\] Initial Key Serial Number \\(KSN\\): FFFF00A0200001E00000[\r\n]"
		"\t\\[KC\\] Key Check Value \\(KCV\\) of wrapped key: 0169E3 \\(Legacy KCV algorithm\\)[\r\n]"
		"\t\\[KP\\] Key Check Value \\(KCV\\) of KBPK: ECAD62 \\(Legacy KCV algorithm\\)[\r\n]"
	)
	set_tests_properties(tr31_header_test1
		PROPERTIES
			PASS_REGULAR_EXPRESSION ${tr31_header_test1_regex}
	)
endif()
