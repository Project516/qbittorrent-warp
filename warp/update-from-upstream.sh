#!/usr/bin/env bash
#
# update-from-upstream.sh - move the WARP fork onto a newer qBittorrent release.
#
# The fork is a small set of commits on top of an upstream release tag. This
# fetches upstream, then rebases the fork branch onto the requested release tag
# (the latest stable release-X.Y.Z by default). Resolve any conflicts, finish
# the rebase, and rebuild.
#
set -euo pipefail

UPSTREAM_URL="https://github.com/qbittorrent/qBittorrent.git"
BRANCH="${QBT_WARP_BRANCH:-warp}"

cd "$(git rev-parse --show-toplevel)"

if ! git remote get-url upstream >/dev/null 2>&1; then
    echo "Adding 'upstream' remote -> ${UPSTREAM_URL}"
    git remote add upstream "$UPSTREAM_URL"
fi

echo "Fetching upstream tags..."
git fetch upstream --tags --prune

TARGET="${1:-}"
if [[ -z "$TARGET" ]]; then
    TARGET="$(git tag -l 'release-*' \
        | grep -E '^release-[0-9]+\.[0-9]+\.[0-9]+$' \
        | sort -V | tail -1)"
fi
[[ -n "$TARGET" ]] || { echo "Could not determine a release tag to use." >&2; exit 1; }

if ! git rev-parse -q --verify "refs/tags/${TARGET}" >/dev/null; then
    echo "Tag '${TARGET}' not found after fetch." >&2
    exit 1
fi

CURRENT_BASE="$(git merge-base "$BRANCH" "$TARGET" 2>/dev/null || true)"
if [[ "$CURRENT_BASE" == "$(git rev-parse "$TARGET")" ]]; then
    echo "Branch '${BRANCH}' is already based on ${TARGET}. Nothing to do."
    exit 0
fi

echo "Rebasing '${BRANCH}' onto ${TARGET}..."
git checkout "$BRANCH"
# The new release tag already contains the old base's history, so a plain rebase
# replays only the fork's own commits on top of it.
git rebase "$TARGET"

echo
echo "Rebased onto ${TARGET}. Rebuild with:"
echo "    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build"
