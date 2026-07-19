#!/bin/bash
# Covering up a crime utility
set -e

cur_b=$(git branch --show-current)

git checkout --orphan t_b
git add -A
git commit -m "init"

git branch -D "$cur_b"
git branch -m main

git push -f origin main
