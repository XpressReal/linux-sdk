#!/bin/bash
# Ensure the kernel source is pristine whenever the kernel feature set changes.
#
# linux-avengers.inc has a do_patch:prepend that copies avengers-kmeta/bsp/
# over ${S} and commits it on every do_patch. ${S} is the shared
# work-shared/<machine>/kernel-source tree, so when a later image changes
# KERNEL_FEATURES (xdesktop adds 2KUI, which pulls in features/linux/linux.scc)
# do_patch re-runs against an already patched tree: the bsp copy reverts the
# feature patches that touch those files, and kgit-s2q then resumes past them
# instead of re-applying them. Cleaning first makes do_patch start from
# pristine source, where the whole series applies in order.
#
# Usage: prepare-kernel.sh <featset-name> [extra bitbake args...]
# Run from the build directory, after sourcing oe-init-build-env.
set -e

featset="$1"; shift
stamp="$PWD/.kernel-featset"
previous="$(cat "$stamp" 2>/dev/null || echo none)"

if [ "$previous" = "$featset" ]; then
    echo "kernel feature set unchanged ($featset), keeping existing source"
    exit 0
fi

echo "kernel feature set ${previous} -> ${featset}: cleaning linux-yocto"
bitbake "$@" -c clean linux-yocto
echo "$featset" > "$stamp"
