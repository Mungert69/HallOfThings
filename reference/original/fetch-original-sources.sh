#!/usr/bin/env bash
set -euo pipefail

BASE='https://www.desdes.com/products/oldfiles'
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

command -v curl >/dev/null 2>&1 || {
    echo 'curl is required' >&2
    exit 1
}

for file in \
    halls.asm \
    halls83_chars.rtf \
    halls83_front.rtf \
    halls83_main.rtf
do
    echo "Fetching ${file}"
    curl --fail --location --retry 3 \
        --output "${HERE}/${file}" \
        "${BASE}/${file}"
done

echo "Original reference sources downloaded to ${HERE}"
