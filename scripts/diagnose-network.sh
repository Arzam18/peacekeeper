#!/usr/bin/env bash
# Non-destructive diagnostics for Peacekeeper's checked-in NNUE network.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
mkdir -p dist
REPORT="dist/network-diagnostics.txt"
{
  echo "Peacekeeper NNUE diagnostics"
  echo "Timestamp (UTC): $(date -u +'%Y-%m-%dT%H:%M:%SZ')"
  echo "Source commit: $(git rev-parse HEAD 2>/dev/null || echo unavailable)"
  echo "Source branch: $(git branch --show-current 2>/dev/null || echo unavailable)"
  echo "Latest commit touching network:"
  git log -1 --format='%H %cs %s' -- src/default.nn 2>/dev/null || true
  echo
  echo "Expected network bytes: 18883588"
  echo "Expected network SHA-256: 4e65641f0a32034822d514f959a7ac101cc70491fb4dfdd4a8368a9c6e7524d8"
  echo
  if [[ ! -f src/default.nn ]]; then
    echo "Network status: MISSING"
  else
    echo "Actual network bytes: $(stat -c '%s' src/default.nn 2>/dev/null || wc -c < src/default.nn)"
    echo "Actual network SHA-256: $(sha256sum src/default.nn | awk '{print $1}')"
    echo "File identification: $(file -b src/default.nn 2>/dev/null || echo unavailable)"
    if git ls-files --error-unmatch src/default.nn >/dev/null 2>&1; then
      echo "Git tracking: tracked"
    else
      echo "Git tracking: untracked or ignored"
    fi
    echo "Git status:"
    git status --short -- src/default.nn 2>/dev/null || true
    echo "Blob in HEAD:"
    git rev-parse HEAD:src/default.nn 2>/dev/null || echo "not present in HEAD"
  fi
  echo
  echo "This diagnostic does not modify or replace the network."
} | tee "$REPORT"
