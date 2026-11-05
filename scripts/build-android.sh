#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

NDK_VERSION="27.2.12479018"
API="24"
ARCH="aarch64"
TARGET="${ARCH}-linux-android${API}"
EXPECTED_NET_BYTES="18883588"
EXPECTED_NET_SHA256="13cc994d1ce31406f3000d4013b8a41c8ab47e4baa3ee3a54562e208de2c5e7b"
OUTPUT="$ROOT/build/peacekeeper-android-arm64-neon"
OBJ_DIR="$ROOT/build/obj"
DIST_DIR="$ROOT/dist"

if [[ -z "${ANDROID_NDK_HOME:-}" ]]; then
  echo "ERROR: ANDROID_NDK_HOME is not set." >&2
  exit 2
fi
NDK="$(cd "$ANDROID_NDK_HOME" && pwd)"
if [[ ! -d "$NDK/toolchains/llvm/prebuilt/linux-x86_64" ]]; then
  echo "ERROR: Android NDK toolchain not found under: $NDK" >&2
  exit 2
fi

TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
CXX="${CXX:-$TOOLCHAIN/bin/${TARGET}-clang++}"
READELF="$TOOLCHAIN/bin/llvm-readelf"
OBJDUMP="$TOOLCHAIN/bin/llvm-objdump"
STRIP="$TOOLCHAIN/bin/llvm-strip"

for tool in "$CXX" "$READELF" "$OBJDUMP" "$STRIP"; do
  [[ -x "$tool" ]] || { echo "ERROR: required NDK tool not found: $tool" >&2; exit 2; }
done

NETWORK="$ROOT/src/default.nn"
if [[ ! -f "$NETWORK" ]]; then
  cat >&2 <<'EOF'
ERROR: src/default.nn is missing.
Both supplied source archives contain the same trained network. Restore the
original file at src/default.nn from your Peacekeeper source archive/repository.
This build intentionally will not substitute or generate network weights.
EOF
  exit 3
fi
ACTUAL_NET_BYTES="$(stat -c '%s' "$NETWORK")"
if [[ "$ACTUAL_NET_BYTES" != "$EXPECTED_NET_BYTES" ]]; then
  echo "ERROR: src/default.nn is $ACTUAL_NET_BYTES bytes; expected $EXPECTED_NET_BYTES bytes for this NNUE layout." >&2
  exit 3
fi
ACTUAL_NET_SHA256="$(sha256sum "$NETWORK" | awk '{print $1}')"
if [[ "$ACTUAL_NET_SHA256" != "$EXPECTED_NET_SHA256" ]]; then
  echo "ERROR: src/default.nn SHA-256 does not match the supplied original/Android-port network." >&2
  echo "Expected: $EXPECTED_NET_SHA256" >&2
  echo "Actual:   $ACTUAL_NET_SHA256" >&2
  exit 3
fi

mkdir -p "$OBJ_DIR" "$DIST_DIR"
rm -f "$OBJ_DIR"/*.o "$OUTPUT" "$OUTPUT.sha256" "$DIST_DIR"/nnue-neon-disassembly.txt

CXXFLAGS=(
  -std=c++17
  -O3
  -ffast-math
  -DNDEBUG
  -pthread
  -march=armv8-a+simd
  -fno-strict-aliasing
  -fPIE
  -DVERSION=-1
  '-DNETWORK_FILE="src/default.nn"'
)

printf '\n=== Peacekeeper Android ARM64 NEON ===\n'
printf 'NDK: %s\nAPI: %s\nNetwork: %s bytes\nNetwork SHA-256: %s\n' "$NDK_VERSION" "$API" "$ACTUAL_NET_BYTES" "$ACTUAL_NET_SHA256"
sha256sum "$NETWORK"
"$CXX" --version | head -n 1

shopt -s nullglob
SOURCES=(src/*.cpp)
if [[ ${#SOURCES[@]} -eq 0 ]]; then
  echo "ERROR: no C++ source files found under src/." >&2
  exit 4
fi
OBJECTS=()
for source in "${SOURCES[@]}"; do
  base="$(basename "${source%.cpp}")"
  object="$OBJ_DIR/$base.o"
  echo "=== Compile $source ==="
  "$CXX" "${CXXFLAGS[@]}" -c "$source" -o "$object"
  OBJECTS+=("$object")
done

NNUE_OBJECT="$OBJ_DIR/nnue.o"
if [[ ! -f "$NNUE_OBJECT" ]]; then
  echo "ERROR: nnue.cpp did not produce build/obj/nnue.o." >&2
  exit 4
fi

echo "=== Link Android executable ==="
"$CXX" \
  -pthread \
  -fPIE -pie \
  -static-libstdc++ \
  -Wl,--no-undefined \
  "${OBJECTS[@]}" \
  -lm \
  -o "$OUTPUT"

"$STRIP" --strip-unneeded "$OUTPUT"

printf '\n=== Verify Android AArch64 ELF ===\n'
"$READELF" -h "$OUTPUT" > "$DIST_DIR/elf-header.txt"
if ! grep -Eiq 'Machine:[[:space:]]+AArch64' "$DIST_DIR/elf-header.txt"; then
  cat "$DIST_DIR/elf-header.txt" >&2
  echo "ERROR: output is not AArch64." >&2
  exit 5
fi
if ! grep -Eq 'Type:[[:space:]]+DYN' "$DIST_DIR/elf-header.txt"; then
  echo "ERROR: output is not a position-independent Android executable (PIE)." >&2
  exit 5
fi

printf '\n=== Verify NEON arithmetic in nnue.o ===\n'
"$OBJDUMP" -d "$NNUE_OBJECT" > "$DIST_DIR/nnue-neon-disassembly.txt"
for pattern in \
  '[[:space:]]add[[:space:]]+v[0-9]+\.8h' \
  '[[:space:]]sub[[:space:]]+v[0-9]+\.8h' \
  '[[:space:]]smull2?[[:space:]]+v[0-9]+\.4s' \
  '[[:space:]]addp[[:space:]]+v[0-9]+\.4s' \
  '[[:space:]]addv[[:space:]]+s[0-9]+,[[:space:]]v[0-9]+\.4s'; do
  if ! grep -Eiq "$pattern" "$DIST_DIR/nnue-neon-disassembly.txt"; then
    echo "ERROR: expected NEON NNUE instruction pattern not found: $pattern" >&2
    echo "Disassembly saved to $DIST_DIR/nnue-neon-disassembly.txt" >&2
    exit 6
  fi
done
echo "PASS: NNUE object contains 8x16-bit ADD/SUB, widening multiply, pairwise ADD, and horizontal ADD."

printf '\n=== Verify runtime dependencies ===\n'
"$READELF" -d "$OUTPUT" > "$DIST_DIR/dynamic-section.txt"
if grep -Fq 'libc++_shared.so' "$DIST_DIR/dynamic-section.txt"; then
  echo "ERROR: executable still depends on libc++_shared.so." >&2
  exit 7
fi
echo "PASS: no libc++_shared.so dependency."

sha256sum "$OUTPUT" > "$OUTPUT.sha256"
(
  cd "$ROOT/build"
  zip -9 -q peacekeeper-android-arm64-neon.zip "$(basename "$OUTPUT")" "$(basename "$OUTPUT").sha256"
)
cp "$OUTPUT.sha256" "$DIST_DIR/peacekeeper-android-arm64-neon.sha256"

printf '\n=== Build complete ===\n'
file "$OUTPUT"
ls -lh "$OUTPUT" "$ROOT/build/peacekeeper-android-arm64-neon.zip"
cat "$OUTPUT.sha256"
