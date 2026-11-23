#!/usr/bin/env bash
# usage: check-no-heap.sh <object> <symbol>
# fails if the object needs any function outside the allowlist, or doesn't define the symbol
# an allowlist, since heap and exception support arrive through helpers a denylist can't name
set -euo pipefail
object=$1
symbol=$2

defined=$(arm-none-eabi-nm --defined-only "$object" | awk '{ print $NF }')
if ! grep -qE "^(${symbol}|_Z${#symbol}${symbol}.*)$" <<<"$defined"; then
    echo "$object doesn't define $symbol, so the check would prove nothing" >&2
    exit 1
fi

# libc's memory routines, plus gcc's arm helpers for division, shifts and switch tables
allowed='^(memcpy|memmove|memset|memcmp|__aeabi_[a-z0-9_]+|__gnu_thumb1_case_[a-z0-9]+)$'
undefined=$(arm-none-eabi-nm --undefined-only "$object" | awk '{ print $NF }')
found=$(grep -vE "$allowed" <<<"$undefined" || true)
if [[ -n $found ]]; then
    echo "$object needs functions outside the allowlist:" >&2
    echo "$found" >&2
    exit 1
fi
echo "$object: needs only allowlisted functions"
