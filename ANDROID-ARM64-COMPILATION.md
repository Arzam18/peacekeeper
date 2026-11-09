# Peacekeeper — Android ARM64 NEON compilation guide

## Scope

This guide documents the baseline Android ARM64 build for the Peacekeeper source in the user's `Arzam18/peacekeeper` fork. The baseline target is AArch64 Android API 24 with mandatory AArch64 Advanced SIMD (NEON). This is the first build stage; it does not claim ARM DotProd support.

## Required source and model

- Repository root: `peacekeeper-main/`
- C++ sources: `src/*.cpp`
- NEON implementation: `src/simd.h`
- NNUE inference and accumulator updates: `src/nnue.cpp`
- NNUE data: `src/default.nn`
- Build script: `scripts/build-android.sh`
- GitHub Actions workflow: `.github/workflows/android.yml`

The verified network source is [`Arzam18/peacekeeper/src/default.nn`](https://github.com/Arzam18/peacekeeper/blob/main/src/default.nn). In the diagnostics run, it was tracked in commit `883249d17e0ec5b3d0f8efa77e9a8fec3d7eab05`, with 18,883,588 bytes and SHA-256:

`4e65641f0a32034822d514f959a7ac101cc70491fb4dfdd4a8368a9c6e7524d8`

The build now pins this fork network identity. Do not create a dummy model, substitute another network, or silently download weights. The build stops if `src/default.nn` is absent or does not match the pinned size and checksum. If the network is intentionally changed later, verify the new model against the matching source before updating the pin.

## Toolchain

- GitHub Actions runner: Ubuntu 24.04
- Android NDK: r27c (`27.2.12479018`)
- Target triple: `aarch64-linux-android24`
- Compiler: NDK `aarch64-linux-android24-clang++`
- Language: C++17
- Baseline architecture flag: `-march=armv8-a+simd`

## Build flags

The workflow compiles every `src/*.cpp` translation unit with:

```sh
-std=c++17
-O3
-ffast-math
-DNDEBUG
-pthread
-march=armv8-a+simd
-fno-strict-aliasing
-fPIE
-DVERSION=0
-DPEACEKEEPER_BUILD_DATE="YYYYMMDD"
-DNETWORK_FILE="src/default.nn"
```

`uci_date` is a required `workflow_dispatch` input. It must contain exactly eight digits (`YYYYMMDD`). The selected value is compiled into the UCI engine name (`Peacekeeper vYYYYMMDD`) and recorded in `dist/build-metadata.txt`.

## Manual build equivalent

From the repository root, after installing NDK r27c and ensuring the model file is present:

```sh
export ANDROID_NDK_HOME=/path/to/android-ndk-r27c
export UCI_DATE=20261109
bash scripts/build-android.sh
```

The script compiles each C++ source to `build/obj/*.o`, links a PIE AArch64 executable, strips it, and verifies the output. The script is the canonical build implementation; do not duplicate its flags in a second workflow.

## Why this is a real NEON build

On AArch64, `src/simd.h` selects `PEACEKEEPER_NEON` and defines `SIMD`. The NNUE code in `src/nnue.cpp` then compiles the vector path rather than its scalar `#else` implementation.

The ARM implementation uses actual intrinsics:

- `vaddq_s16` / `vsubq_s16` — 8-lane signed 16-bit accumulator updates;
- `vminq_s16` / `vmaxq_s16` — signed 16-bit clipping;
- `vmulq_s16` — 8-lane signed 16-bit multiply;
- `vmull_s16` — widening signed 16-bit multiply;
- `vpaddq_s32` — pairwise 32-bit accumulation;
- `vaddvq_s32` — horizontal 32-bit reduction.

The build script disassembles `nnue.o` and fails if the expected NEON arithmetic instruction families are absent. This is stronger evidence than checking compiler flags or preprocessor macros alone.

## Build outputs

The engine-only GitHub Actions artifact contains exactly one executable:

`peacekeeper-android-arm64-neon`

A separate verification artifact contains:

- ELF header and AArch64/PIE checks;
- NNUE object disassembly;
- model size and SHA-256;
- compiler version;
- selected UCI build date and build flags.

The workflow does not run the AArch64 executable on the x86-64 GitHub runner. Install and test the artifact on an ARM64 Android device.

## Runtime checks on Android

1. Run `uci` and confirm `id name Peacekeeper vYYYYMMDD`.
2. Confirm the engine responds with `uciok` and `readyok`.
3. Run `position startpos` followed by `go depth 10`; confirm a legal `bestmove`.
4. Run a fixed-node/fixed-depth comparison against the known-good baseline using the same NNUE SHA, thread count, hash size, and positions.
5. Record NPS separately from playing strength. Do not infer strength from NPS alone.

The startup banner is sent to stderr so it cannot corrupt the UCI stdout stream.

## DotProd status and gate

**DotProd is not implemented in this snapshot.** The current NNUE uses signed 16-bit activations and weights. AArch64 `SDOT`/`UDOT` dot-product instructions operate on 8-bit lanes. Adding `-march=armv8.2-a+dotprod` alone does not make this 16-bit NNUE path a DotProd backend.

Do not publish or label a DotProd artifact until all of the following are true:

1. A mathematically exact DotProd kernel is implemented for the actual network representation, or the network is deliberately converted/retrained with verified equivalent semantics.
2. The scalar/NEON and DotProd kernels pass randomized differential tests against the same reference arithmetic and real network.
3. The DotProd build is compiled separately with `-march=armv8.2-a+dotprod`.
4. Disassembly of the NNUE hot path contains actual `sdot`/`udot` instructions.
5. The baseline NEON build remains a separate, compatible artifact.
6. Android runtime, fixed-node parity, NPS, and tournament strength are measured independently.

A compiler target flag, a macro, or an isolated SDOT instruction outside the NNUE hot path is not sufficient proof.
