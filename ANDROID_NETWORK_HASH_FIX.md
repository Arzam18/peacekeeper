# Peacekeeper NNUE hash-check correction

The user's repository currently reports `src/default.nn` SHA-256:

`4e65641f0a32034822d514f959a7ac101cc70491fb4dfdd4a8368a9c6e7524d8`

The previous script pinned the local recovery reference file hash `13cc...` and therefore rejected a different repository copy. The replacement script no longer assumes that the local reference network is the active repository network. It validates the 18,883,588-byte tensor layout, records the actual SHA-256, and uses the repository file as-is.

To enforce a specific model revision, set the Actions repository variable `PEACEKEEPER_EXPECTED_NET_SHA256` to the intended SHA-256. Leave it unset to build using the file already in the repository. The build script never downloads, overwrites, or generates `src/default.nn`.

The source's raw network format has no magic/version header, so size alone cannot prove that arbitrary bytes are trained weights. Keep the known Peacekeeper network from the intended source revision.
