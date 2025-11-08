#!/usr/bin/env bash
set -euo pipefail
if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  exit 1
  fi
  cd "$(git rev-parse --show-toplevel)"
  git add -A
  if git diff --staged --quiet && git diff --quiet; then
    exit 0
    fi
    msg=${1:-Update}
    git commit -m "$msg" || true
    if ! git remote get-url origin >/dev/null 2>&1; then
      if command -v gh >/dev/null 2>&1 && gh auth status >/dev/null 2>&1; then
          gh repo create --source=. --remote=origin --push
            else
                if [ -n "${BROWSER:-}" ]; then
                      "$BROWSER" "https://github.com/new"
                          fi
                              exit 2
                                fi
                                fi
                                branch=$(git rev-parse --abbrev-ref HEAD)
                                git fetch origin "$branch" || true
                                git pull --rebase origin "$branch" || true
                                git push -u origin "$branch"