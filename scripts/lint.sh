#!/usr/bin/env sh
#
# Lint Duet's own sources, and only those.
#
# Two things this does that `clang-tidy -p build/` does not:
#
# Ninja Multi-Config writes one compile-commands entry per configuration, so the
# plain command lints every file twice — once for Debug and once for Release —
# and the Release pass reports nothing the Debug pass did not. And clang-tidy is
# single-threaded while the files are independent, so a sweep runs them one at a
# time. Filtering the database to Debug and running four at once takes the sweep
# from about eight and a half minutes to about eighty seconds. Four is the same
# cap the build uses, and for the same reason: each process peaks near 1.6 GB.
#
# The filtered database holds Duet's sources and nothing else, so the vendored
# JUCE and Tracktion trees are unreachable by construction rather than by a
# pattern that has to be kept right. It is a directory filter and not
# `git ls-files`, which means a source the tree does not track yet is linted
# like any other instead of being silently skipped.
#
# With no arguments it sweeps everything. With arguments it lints just those
# files, which is what to do while iterating.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tidydb=$root/build/tidy

if [ ! -f "$root/build/compile_commands.json" ]; then
    echo "lint: no build/compile_commands.json — run 'cmake --preset linux-debug' first" >&2
    exit 1
fi

mkdir -p "$tidydb"
python3 - "$root" <<'FILTER'
import json
import sys

root = sys.argv[1]
ours = (f"{root}/modules/", f"{root}/tests/")

with open(f"{root}/build/compile_commands.json") as handle:
    database = json.load(handle)

kept = [
    entry
    for entry in database
    if "/Debug/" in entry.get("output", "") and entry["file"].startswith(ours)
]

with open(f"{root}/build/tidy/compile_commands.json", "w") as handle:
    json.dump(kept, handle)

if not kept:
    sys.exit("lint: no Duet sources in the compile database")
FILTER

if [ "$#" -gt 0 ]; then
    exec clang-tidy-18 -p "$tidydb" -quiet "$@"
fi

exec run-clang-tidy-18 -p "$tidydb" -j 4 -quiet
