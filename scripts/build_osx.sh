#!/bin/bash
BASEDIR=$(dirname $0)/..
CWD=$(pwd)

BUILDDIR=$BASEDIR/builddir/osx

source .venv/bin/activate

rm -rf $BUILDDIR
mkdir -p $BUILDDIR

# the commit the libraries are built from, installed with them (see install_osx.sh)
git -C $BASEDIR describe --always --dirty > $BUILDDIR/rt-xr-maf-native.version

cd $BASEDIR
meson setup --wipe -Davpipeline=true -Davpipeline:avcodec_dir=external/avcodec/osx -Ddebug=true builddir/osx
meson compile -C $BUILDDIR
cd $CWD