#!/bin/bash

set -e 

if [ -d "$ANDROID_SDK_HOME" ]; then 
    echo "ANDROID_NDK_HOME set to '$ANDROID_SDK_HOME'"; 
else 
    echo "ANDROID_SDK_HOME is not set / a valid directory" && exit 1; 
fi

if [ -d "$ANDROID_NDK_HOME" ]; then 
    echo "ANDROID_NDK_HOME set to '$ANDROID_NDK_HOME'"; 
else 
    echo "ANDROID_NDK_HOME is not set / a valid directory" && exit 1; 
fi

BASEDIR=$(dirname $0)/..

# Build ffmpeg for android arm64 with mediacodec support
cd $BASEDIR/../ffmpeg-android-maker

./ffmpeg-android-maker.sh --enable-mediacodec --enable-jni

# Copy the build artifacts to a meson subprojects, 
# so that meson can use it as a fallback to the default lookup method
FFMPEG_DEPENDENCY_DIR=$BASEDIR/subprojects/FFmpeg
rm -rf $FFMPEG_DEPENDENCY_DIR
mkdir -p $FFMPEG_DEPENDENCY_DIR/arm-v8a
cp ./output/lib/arm64-v8a $FFMPEG_DEPENDENCY_DIR/arm-v8a
cp $BASEDIR/scripts/build_ffmpeg_android.meson $FFMPEG_DEPENDENCY_DIR/
