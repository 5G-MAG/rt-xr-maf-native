#!/bin/bash
BASEDIR=$(dirname $0)/..
CWD=$(pwd)

BUILDDIR=$BASEDIR/builddir/osx

source .venv/bin/activate

rm -rf $BUILDDIR
mkdir -p $BUILDDIR

cd $BASEDIR
meson setup --wipe -Davpipeline=true -Davpipeline:avcodec_dir=external/avcodec/osx -Ddebug=true builddir/osx
meson compile -C $BUILDDIR
cd $CWD