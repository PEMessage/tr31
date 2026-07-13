#!/bin/bash
#
# 生成 TR-31 密钥块示例 (Version B, RSA Signature Key)
#   Header: BXXXXS0RS00N0000 + CT optional block (X.509 Certificate)
#
set -euo pipefail

TR31_TOOL="${TR31_TOOL:-/home/zhuojw/workspace/tr31/build/src/tr31-tool}"
WORKDIR="/tmp/tr31_gen_$$"
mkdir -p "$WORKDIR"

# ── 1. 生成 RSA 私钥 (PKCS#8 DER) ─────────────────────────────────
RSA_BITS="${1:-2048}"
echo "[1/5] Generating ${RSA_BITS}-bit RSA key..."
openssl genrsa "${RSA_BITS}" 2>/dev/null | \
  openssl pkcs8 -topk8 -nocrypt -outform DER 2>/dev/null > "$WORKDIR/rsa_key.der"

RSA_KEY_HEX=$(xxd -p "$WORKDIR/rsa_key.der" | tr -d '\n')
echo "      RSA key length: $(wc -c < "$WORKDIR/rsa_key.der") bytes"

# ── 2. 生成自签名 X.509 证书 (PEM 直出) ──────────────────────────
echo "[2/5] Generating self-signed X.509 certificate..."
CERT_PEM=$(openssl req -x509 -new -key "$WORKDIR/rsa_key.der" \
  -inform DER \
  -days 365 -subj "/CN=TR-31 Demo" 2>/dev/null)

CERT_BASE64=$(echo "$CERT_PEM" | sed -n '/BEGIN CERTIFICATE/,/END CERTIFICATE/p' | \
  grep -v -- '-----' | tr -d '\n')
echo "      Certificate base64: ${#CERT_BASE64} chars"

# ── 3. 生成 KBPK (TDES, 16 bytes) ─────────────────────────────────
# Version B 使用 TDES KBPK (16 bytes)
echo "[3/5] Generating TDES KBPK..."
KBPK_HEX=$(openssl rand -hex 16)
echo "      KBPK: ${KBPK_HEX}"

# ── 4. 使用 tr31-tool 生成 TR-31 密钥块 ───────────────────────────
echo "[4/5] Building TR-31 key block..."

# --export-header 说明:
#   "B0100S0RS00N0000"
#     B      = Version B (TR-31:2010 / X9.143)
#     0100   = length placeholder (工具会自动重算, 忽略此值)
#     S0     = key_usage: Asymmetric Key Pair for Digital Signature
#     R      = algorithm: RSA
#     S      = mode_of_use: Signature Only
#     00     = key_version: unused
#     N      = exportability: Not exportable
#     00     = opt_blocks_count placeholder (工具自动调整)
#     00     = key_context(0) + reserved(0)

TR31_OUTPUT=$("$TR31_TOOL" \
  --export="$RSA_KEY_HEX" \
  --kbpk="$KBPK_HEX" \
  --export-header="B0100S0RS00N0000" \
  --export-opt-block-CT-X509="$CERT_BASE64" 2>&1)

echo ""
echo "══════════════ TR-31 Key Block ══════════════"
echo "$TR31_OUTPUT"
echo "═════════════════════════════════════════════"

# ── 5. 验证: 解码回来检查 ─────────────────────────────────────────
echo ""
echo "[5/5] Verifying by decoding..."
"$TR31_TOOL" --import "$TR31_OUTPUT" --kbpk "$KBPK_HEX" 2>&1

echo ""
echo "Done. Work files kept at: $WORKDIR"
