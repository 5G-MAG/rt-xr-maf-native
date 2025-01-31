#!/bin/bash
BASEDIR=$(dirname $0)/..
DESTDIR=$1

ARCH=android-aarch64
BUILDDIR=$BASEDIR/builddir/$ARCH

# copy shared libraries
rm $DESTDIR/bin/android/arm64/*.so
cp $BUILDDIR/subprojects/**/*.so $DESTDIR/bin/android/arm64

# copy C# bindings sources
rm -rf $DESTDIR/bin/maf/swig/*.cs
cp $BASEDIR/subprojects/maf_csharp/swig/*.cs $DESTDIR/maf/swig/

# copy FFmpeg dependency to Unity project
rm $DESTDIR/dependencies/ffmpeg/7.1/android/arm64/*.so
cp $BASEDIR/subprojects/FFmpeg/arm64-v8a/lib/*.so $DESTDIR/dependencies/ffmpeg/7.1/android/arm64
