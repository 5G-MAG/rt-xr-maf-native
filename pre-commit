#!/bin/sh
STASH_NAME="pre-commit-$(date +%s)"
git stash save --quiet --keep-index --include-untracked $STASH_NAME

meson setup --reconfigure -Davpipeline=true -Dcsharp=true -Dlibmaf=true builddir
meson test -C builddir
RESULT=$?

STASHES=$(git stash list)
if [[ $STASHES == *"$STASH_NAME" ]]; then
  git stash pop --quiet
fi

[ $RESULT -ne 0 ] && exit 1
exit 0
