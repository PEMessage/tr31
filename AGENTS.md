# AGENTS.md

C11 library and CLI (`tr31-tool`) implementing payment key blocks for ANSI X9.143, ASC X9 TR-31 and ISO 20038. Despite the broader scope, the API/types/tool are all prefixed `tr31`.

## Prerequisites
- `crypto/` is a git submodule and is required. If empty: `git submodule update --init --recursive`.
- Crypto backend must be installed: MbedTLS (preferred) or OpenSSL.
- `tr31-tool` needs `argp` (Glibc/system/`-DFETCH_ARGP=YES`). Disable with `-DBUILD_TR31_TOOL=OFF`.

## Build & test
```shell
cmake -B build                                  # generates compile_commands.json by default
cmake --build build
ctest --test-dir build --output-on-failure -j 4
```
- Single test: `ctest --test-dir build -R tr31_decrypt_test --output-on-failure`
- Test binaries live under `build/test/` and `build/crypto/test/`.
- Leak/mem check: `ctest --test-dir build -T MemCheck -j 10` (Valgrind).
- Sanitizers: reconfigure with `-DTR31_ENABLE_SANITIZERS=YES` (ASan/UBSan/LSan).
- CI also exercises `-DTR31_USE_SSCANF_DATETIME=YES` (sscanf date/time path) and `-DTR31_ENABLE_HARDENING=YES`.

## Layout
- `src/tr31.{c,h}` — core parse/decrypt and encode/encrypt for key block versions A–E (main API).
- `src/tr31_crypto.c` — key derivation / crypto glue over the `crypto/` abstraction.
- `src/tr31_strings.c` — stringify header attributes (public: `tr31_strings.h`).
- `src/tr31-tool.c` — CLI only; separate license (see below).
- `test/` — four ctest suites: decode, crypto, decrypt, export.
- `crypto/` — submodule providing `crypto_tdes`/`crypto_aes`/`crypto_mem`/`crypto_rand`.

## Conventions & gotchas
- Indentation is tabs.
- Do NOT add comments unless present in surrounding code.
- Licensing differs by file: library is LGPL v2.1, but `src/tr31-tool.c` is GPL v3. Preserve the existing per-file license headers.
- When editing a file with a copyright header, bump the year to the current year. `scripts/pre-commit` blocks commits whose staged files carry an outdated copyright year; install it via `cp scripts/pre-commit .git/hooks/pre-commit`.
