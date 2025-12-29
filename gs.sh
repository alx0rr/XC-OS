#!/usr/bin/env bash
set -euo pipefail

# git-sync.sh
# Usage:
#   ./git-sync.sh push ["commit message"]   # stage all, commit (with message or default) and push to origin/main
#   ./git-sync.sh fetch                      # fetch all remotes and pull (rebase + autostash)

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
# Use an array so arguments are passed safely (prevents embedded-quote issues)
GIT_CMD=(git -C "$REPO_ROOT")

print_usage() {
  cat <<EOF
Usage: $(basename "$0") <push|fetch> [commit-message]

Commands:
  push [message]   Stage all changes, commit (message or auto) and push to origin/main
  fetch            Fetch all remotes and pull --rebase --autostash

Examples:
  $(basename "$0") push "Save local changes"
  $(basename "$0") fetch
EOF
}

if [ $# -lt 1 ]; then
  print_usage
  exit 2
fi

cmd="$1"
shift || true

case "$cmd" in
  push)
    # Check for any changes
    changes="$(${GIT_CMD[@]} status --porcelain)"
    if [ -z "$changes" ]; then
      echo "No changes to commit."
      exit 0
    fi

    msg="${*:-}"  # remaining args joined
    if [ -z "$msg" ]; then
      msg="Auto-commit: $(date -u +"%Y-%m-%d %H:%M:%SZ")"
    fi

    echo "Staging all changes..."
    ${GIT_CMD[@]} add -A

    echo "Committing..."
    if ${GIT_CMD[@]} commit -m "$msg"; then
      echo "Commit created. Pushing to origin/main..."
      if ${GIT_CMD[@]} push origin main; then
        echo "Push successful."
      else
        echo "Push failed." >&2
        exit 3
      fi
    else
      echo "Nothing to commit or commit failed." >&2
      exit 4
    fi
    ;;

  fetch)
    echo "Fetching remotes and pulling latest changes (rebase + autostash)..."
    ${GIT_CMD[@]} fetch --all --prune
    ${GIT_CMD[@]} pull --rebase --autostash
    echo "Fetch & pull complete."
    ;;

  -h|--help)
    print_usage
    ;;

  *)
    echo "Unknown command: $cmd" >&2
    print_usage
    exit 2
    ;;
esac

exit 0
