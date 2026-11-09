# Falcon — Android ARM64 engine compilation rules

These rules apply to every engine added to Falcon. They are written for both human maintainers and coding agents.

## 1. Preserve the complete source set

- Work on one engine at a time.
- Keep every previously integrated engine in the full Falcon source reference.
- Do not replace unrelated engine folders while updating one engine.
- Keep original source snapshots and network assets unchanged unless the task explicitly requires a change.
- Deliver a complete Falcon source archive and a separate modified-files-only overlay. The overlay must contain only changed/new files and preserve repository-relative paths.
- Do not claim a build, runtime test, or strength test passed unless it was actually run. Distinguish source inspection, host compilation, GitHub Actions, and physical-device testing.

## 2. Required version/date input

Every engine's manual GitHub Actions workflow must include a required `workflow_dispatch` input named `uci_date` (or a clearly engine-specific equivalent):

```yaml
inputs:
  uci_date:
    description: "ENGINE UCI version/date (YYYYMMDD)"
    required: true
    default: "YYYYMMDD"
    type: string
```

Validate exactly eight decimal digits. Pass the value into the engine's UCI identity/build version wherever the engine supports it, and always record it in build metadata and the artifact name. Never hardcode the selected build date only in the source.

## 3. Inspect existing ARM/NEON before adding DotProd

Before changing SIMD code:

1. Find the engine's architecture detection and every scalar/SIMD compile-time branch.
2. Trace the NNUE/evaluation hot path from the UCI search into the actual kernel.
3. Confirm that the AArch64 build selects the intended NEON branch, not a scalar fallback.
4. Inspect the operations and operand widths, signedness, saturation, rounding, and accumulator behavior.
5. Compile the baseline and inspect its disassembly. A `-march` flag or `__ARM_NEON` macro alone is not sufficient proof.
6. If a path is only partially vectorized, document which loops remain scalar. Do not call the entire engine fully NEON-accelerated without qualification.

## 4. Build and publish baseline first

Build the compatible baseline first:

- Android NDK r27c (`27.2.12479018`) unless the engine requires a documented alternative.
- Android API 24 unless source compatibility requires another API.
- Target `aarch64-linux-android24`.
- Baseline flag normally `-march=armv8-a+simd`.
- Build one engine-only executable artifact. Keep reports/checksums in a separate verification artifact.
- Verify AArch64 ELF, PIE, runtime dependencies, model identity, UCI protocol, and actual NEON instructions.
- Upload the baseline artifact before starting the DotProd variant.

## 5. DotProd is a separate implementation and artifact

- Target flag normally `-march=armv8.2-a+dotprod`.
- Do not assume a compiler flag automatically vectorizes an existing scalar or NEON kernel.
- Use actual DotProd instructions (`sdot`/`udot`) in the engine's relevant hot path.
- Respect the network's real numeric format. SDOT/UDOT are 8-bit dot products; they are not drop-in replacements for signed 16-bit multiply/accumulate.
- Preserve exact arithmetic unless a deliberate quantization/network conversion is supported by validated network semantics.
- Add randomized differential tests against scalar/NEON reference code, plus tests using the real network.
- Disassemble the final DotProd object/binary and verify DotProd instructions are in the NNUE hot path.
- Upload a separate DotProd executable artifact. Never replace the baseline artifact.
- If a real DotProd kernel cannot be implemented safely, report that limitation and keep only the baseline artifact; do not publish a fake DotProd build.

## 6. Verification and status

Track these independently for every engine and variant:

- Source audit complete;
- Build success;
- AArch64 ELF and dependency checks;
- NEON/DotProd disassembly checks;
- Network size/hash and loader verification;
- UCI handshake and search runtime on Android;
- NPS on the same device and settings;
- Fixed-node/fixed-depth parity;
- Playing strength / tournament result.

Successful compilation does not establish runtime correctness or playing strength. Keep an engine marked Testing until the required device and strength checks are complete.
