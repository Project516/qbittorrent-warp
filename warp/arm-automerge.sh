#!/usr/bin/env bash
# Usage: arm-automerge.sh <pr> <squash|merge>. Needs GH_TOKEN and GH_REPO.
# Arms auto-merge only when the base branch has a ruleset requiring a pull
# request and status checks. Without them GitHub merges immediately.
set -euo pipefail

pr="$1"
method="$2"

base="$(gh pr view "$pr" --json baseRefName --jq .baseRefName)"
types="$(gh api "repos/${GH_REPO}/rules/branches/${base}" --jq '[.[].type]')"
for need in pull_request required_status_checks; do
  jq -e --arg t "$need" 'index($t)' <<<"$types" >/dev/null \
    || { echo "::error::${base} has no ${need} rule, so auto-merge would not wait. Not arming."; exit 1; }
done

[ "$(gh pr view "$pr" --json autoMergeRequest --jq '.autoMergeRequest != null')" = "true" ] && exit 0

gh pr merge "$pr" --auto "--${method}"
