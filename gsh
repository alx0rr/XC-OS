#!/bin/bash
br=$(git branch --show-current)
git add .
git commit -m "${1:-upd}"
git push origin "$br"