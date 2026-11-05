# Peacekeeper Android recovery — original vs Android port

## Source revisions compared

- Original: `peacekeeper-main(original).zip`, archive commit `e5100c7ef87305c2a8ca07002d06a2031fa401e2`.
- Android-added: `peacekeeper-main (android-added).zip`, archive commit `4c28e277cd112047b1f79960efb647169452e8f1`.
- The source-tree diff shows the Android-added archive changes only `Makefile`, `src/main.cpp`, and `src/simd.h`, adds `src/stack_thread.h`, and adds an Android workflow. Search/evaluation implementation files outside those areas are unchanged.

## Trained network identity

Both supplied archives DO contain `src/default.nn`. They are byte-for-byte identical:

- Size: **18,883,588 bytes**.
- SHA-256: `13cc994d1ce31406f3000d4013b8a41c8ab47e4baa3ee3a54562e208de2c5e7b`.

The Android build script now checks both size and SHA-256. Keep this exact network in the repository. The modified-files-only ZIP intentionally does not replace it; the complete-source ZIP retains it.

## Findings

### 1. The stack issue is real, and the stack fix is strength-neutral

`NNUE` contains `std::array<Accumulator, 128>`. Each `Accumulator` contains two 1536-element `int16_t` arrays. The measured `sizeof(NNUE)` is **786,436 bytes** (about 768 KiB). `iterative_deepening()` originally constructs `NNUE nnue` as a local variable on the search worker's stack. That consumes most of a typical 1 MiB Android pthread stack before recursive PVS/quiescence frames and other locals are counted.

The upstream Makefile's `-Wl,--stack,33554432` and the MSVC linker pragma indicate a 32 MiB stack intention for the executable, but they do not set the default stack size of every Android `std::thread`. The Android port's `StackThread` uses `pthread_attr_setstacksize()` to give engine-owned POSIX threads a 32 MiB stack. This is a crash-prevention change; it does not change pruning, reductions, extensions, move ordering, evaluation weights, or time management.

This recovery retains the 32 MiB stack and additionally moves the ~786 KiB NNUE local in `iterative_deepening()` and the `eval` command to `std::make_unique<NNUE>()`. The object remains alive for the complete search/evaluation call; only its storage location changes. The wrapper falls back to `std::thread` on Windows so the original Windows build is not broken by the POSIX header.

### 2. No search-strength change appears in the source diff

The diff does not alter PVS, quiescence, pruning, reductions, search extensions, history tables, time management, evaluation constants, or NNUE weights. The Android additions replace thread creation to assign a larger stack and add a separate NEON backend.

The Android Makefile retains the original performance-relevant options `-O3`, `-ffast-math`, `-DNDEBUG`, and `-pthread`; it replaces the desktop-only `-march=native` / `-static` / Windows stack linker option with an Android-compatible target. The Android build adds `-fno-strict-aliasing` because the existing NNUE SIMD interface type-puns its aligned integer buffers as vector registers.

### 3. NEON arithmetic matches the scalar/x86 operations

The ARM backend maps the signed-16-bit operations to AArch64 NEON:

- accumulator ADD/SUB: `vaddq_s16` / `vsubq_s16`;
- clipped activation square: `vmulq_s16`;
- signed widening multiply: `vmull_s16` on low and high halves;
- pairwise 32-bit accumulation: `vpaddq_s32`;
- final four-lane sum: `vaddvq_s32`.

The hidden width is 1536, divisible by the NEON eight-lane width. The `register_madd_16` layout matches the adjacent-pair sums from `_mm256_madd_epi16` / `_mm512_madd_epi16`. Clipped activation values are at most 181, so `181*181=32761` fits in signed 16-bit. There is no SDOT/Dotprod implementation here; the network uses signed 16-bit arithmetic.

Checks completed:

- A 1,000-case randomized arithmetic reference test passes for accumulator updates and the NNUE output accumulation.
- A second reference test using the actual trained network's hidden weights passes 100 randomized NNUE output cases modulo the 32-bit accumulator semantics.
- Isolated AArch64 Clang code generation emits 8x16-bit `add`/`sub`, signed widening multiply, pairwise add, and horizontal add instructions.

These checks support arithmetic equivalence. They do not measure the NEON implementation's NPS on the Galaxy A16.

### 4. Real-network host differential isolates the stack/heap change

Original and recovery trees were compiled on the same Linux host in scalar mode (`-march=x86-64`) with the same C++17/O3/fast-math flags and the identical trained network SHA above. This avoids comparing two different host SIMD widths and isolates the Android thread/heap-storage changes.

Fixed-depth UCI searches returned identical depth-by-depth scores, node counts, PVs, and best moves for:

- start position through depth 7;
- a middlegame FEN through depth 7;
- an endgame FEN through depth 8 in the first successful comparison run.

Elapsed milliseconds and NPS naturally differed between runs due to host scheduling and startup overhead; the search data (scores/nodes/PV/bestmove) matched. This shows the thread-stack and heap-allocation changes do not weaken the search in fixed-node/depth comparisons.

### 5. What is and is not established about the reported strength drop

**The larger stack is not a plausible cause of lower playing strength.** It only prevents stack exhaustion. Moving NNUE storage to the heap is also behavior-preserving.

**The NEON implementation has no identified arithmetic mismatch** and its integer operations are equivalent to the x86/scalar formulation. However, fixed-depth tests above were run on host scalar binaries, not on the actual Android NEON executable. The Android NEON binary still needs a GitHub Actions build and an on-device fixed-node comparison.

For the remaining strength investigation, compare these on the same phone and with the same `src/default.nn` SHA, `Threads=1`, Hash size, position set, and engine options:

1. fixed-node or fixed-depth results (detects any evaluation/search divergence);
2. NPS on the same positions (detects speed loss);
3. only then fixed-time match results (measures practical strength).

If fixed-node scores and PVs match but fixed-time Elo differs, investigate throughput and time management rather than search logic. A different NNUE SHA, thread count, Hash size, or compile flags invalidates the comparison.

## Android build design

- Android NDK r27c `27.2.12479018`.
- API 24, AArch64 baseline NEON (`-march=armv8-a+simd`).
- C++17, `-O3 -ffast-math -DNDEBUG -pthread -fno-strict-aliasing`.
- One Android UCI executable; no GUI.
- The workflow downloads the official NDK directly and is manual (`workflow_dispatch`).
- The build script validates the exact trained network size and SHA-256, AArch64 PIE ELF, NEON instructions in `nnue.o`, and absence of `libc++_shared.so`.

## Remaining verification

- Actual NDK r27c GitHub Actions build: **pending**.
- Galaxy A16 UCI runtime: **pending**.
- On-device NEON fixed-node comparison against the original scalar build: **pending**.

Do not call this build strength-restored until those device-level comparisons and tournament testing are completed.
