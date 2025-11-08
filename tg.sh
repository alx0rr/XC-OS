#!/usr/bin/env bash
set -euo pipefail
if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "Не git репозиторий"
    exit 1
    fi
    cd "$(git rev-parse --show-toplevel)"
    git status --porcelain
    git add -A
    msg=${1:-"Update"}
    if git diff --staged --quiet; then
      echo "Нет изменений для коммита"
      else
        git commit -m "$msg"
        fi
        if git remote get-url origin >/dev/null 2>&1; then
          :
          else
            if command -v gh >/dev/null 2>&1 && gh auth status >/dev/null 2>&1; then
                gh repo create --source=. --remote=origin --push
                  else
                      "$BROWSER" "https://github.com/new"
                          echo "Создайте репозиторий и добавьте remote origin, затем запустите скрипт снова"
                              exit 1
                                fi
                                fi
                                branch=$(git rev-parse --abbrev-ref HEAD)
                                git pull --rebase origin "$branch" || true
                                git push -u origin "$branch"