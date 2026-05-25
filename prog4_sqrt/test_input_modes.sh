#!/usr/bin/env bash
set -euo pipefail

make >/dev/null

output=$(./sqrt best)

if ! grep -q "Input mode: best" <<<"$output"; then
    echo "expected ./sqrt best to report 'Input mode: best'"
    echo "$output"
    exit 1
fi
