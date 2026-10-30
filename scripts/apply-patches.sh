#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
angle_source="${1:?Usage: bash scripts/apply-patches.sh /path/to/angle}"

for patch in "$script_dir"/../patches/*.patch; do
    if git -C "$angle_source" apply --check "$patch" >/dev/null 2>&1; then
        git -C "$angle_source" apply "$patch"
        echo "Applied $(basename "$patch")"
    elif git -C "$angle_source" apply --reverse --check "$patch" >/dev/null 2>&1; then
        echo "Already present: $(basename "$patch")"
    else
        echo "Cannot apply $(basename "$patch") to $angle_source" >&2
        git -C "$angle_source" apply --check "$patch"
        exit 1
    fi
done
