#!/bin/bash
BASEDIR=$(dirname $0)/..
CWD=$(pwd)

if [ -d "$BASEDIR/venv" ]; then
    # activate the virtual environment used by install_meson.sh
    # on windows, assume gitbash
    if [[ "$OSTYPE" == "msys" ]]; then
        source $BASEDIR/venv/Scripts/activate
    else
        # mac / linux
        source $BASEDIR/venv/bin/activate
    fi
fi

ARCH=android-aarch64
BUILDDIR=$BASEDIR/builddir/$ARCH

rm -rf $BUILDDIR
mkdir -p $BUILDDIR

cd $BASEDIR
meson setup --wipe -Davpipeline -Davpipeline:avcodec_dir=avcodec/android/arm64-v8a -Ddebug=true $BUILDDIR --cross-file $BASEDIR/crossfile/$ARCH
meson compile -C $BUILDDIR
cd $CWD