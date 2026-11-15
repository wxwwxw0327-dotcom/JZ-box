#!/usr/bin/env bash
# 用法: ./push.sh <你的 GitHub PAT>
set -e
TOKEN="${1:-$GITHUB_TOKEN}"
[ -z "$TOKEN" ] && { echo "用法: ./push.sh <PAT>"; exit 1; }
cd "$(dirname "$0")"
git remote set-url origin "https://${TOKEN}@github.com/wxwwxw0327-dotcom/JZ-box.git"
git push origin --all
git remote set-url origin "https://github.com/wxwwxw0327-dotcom/JZ-box.git"
echo "完成: https://github.com/wxwwxw0327-dotcom/JZ-box/branches"
