#!/bin/sh
# Local macOS build and offline tests. No agent CLI or release publishing.
set -eu
if [ "$(uname -s)" != Darwin ]; then
    echo 'This script requires macOS.' >&2
    exit 1
fi
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=${1:-"$root/build-macos"}
configuration=${CONFIGURATION:-Release}
cmake -S "$root" -B "$build" -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE="$configuration"
cmake --build "$build" --parallel
# Keep temporary projects inside the build tree (also works in restricted environments).
build=$(CDPATH= cd -- "$build" && pwd)
mkdir -p "$build/test-tmp"
TMPDIR="$build/test-tmp" ctest --test-dir "$build" --output-on-failure
printf 'Ready: %s/AgentNodify.app\n' "$build"
