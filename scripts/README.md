# Peacekeeper build scripts

This directory contains build-time helper scripts. It is not part of the chess
engine runtime and its contents are not required on the phone.

## `build-android.sh`

Called by `.github/workflows/android.yml` after the workflow installs Android
NDK r27c. It:

1. Checks that `src/default.nn` exists and has the exact byte length required
   by the tensor dimensions declared in `src/nnue.h` (18,883,588 bytes).
2. Calculates and records the network SHA-256. By default it uses the network
   already present in the checked-out repository; it never downloads, replaces,
   or generates weights.
3. Optionally compares the hash against `PEACEKEEPER_EXPECTED_NET_SHA256` when
   that environment variable is explicitly configured.
4. Compiles all `src/*.cpp` files for Android AArch64 with NEON, links the UCI
   executable, then checks the ELF/PIE type, expected NEON instructions, and
   runtime library dependencies.
5. Packages the executable and its checksum into `build/`.

Run locally with `ANDROID_NDK_HOME` pointing to an installed NDK r27c:

```sh
bash scripts/build-android.sh
```

To pin a specific network revision, set the expected SHA-256 first:

```sh
PEACEKEEPER_EXPECTED_NET_SHA256=<sha256> bash scripts/build-android.sh
```

The build script does not certify that arbitrary same-sized bytes are trained
weights. Only use a `src/default.nn` file belonging to the Peacekeeper model
revision being built. The script records its hash so builds can be reproduced.
