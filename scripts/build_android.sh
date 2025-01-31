#!/bin/bash
BASEDIR=$(dirname $0)/..
CWD=$(pwd)

if [ -d "$BASEDIR/venv" ]; then
    source $BASEDIR/venv/bin/activate
fi

ARCH=android-aarch64
BUILDDIR=$BASEDIR/builddir/$ARCH

rm -rf $BUILDDIR
mkdir -p $BUILDDIR

cd $BASEDIR
meson setup --wipe -Ddebug=true $BUILDDIR --cross-file $BASEDIR/crossfile/$ARCH
meson compile -C $BUILDDIR
cd $CWD