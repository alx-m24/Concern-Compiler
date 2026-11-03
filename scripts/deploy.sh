#!/usr/bin/env bash
set -e

if [ -z "$1" ]; then
  echo "Usage: deploy.sh <commit-msg>"
  exit 1
fi

./scripts/build.sh

echo

git add .
git commit -m "$1"
git push origin main
