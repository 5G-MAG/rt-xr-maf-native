BASEDIR=$(readlink -f $(dirname $0)/..)
DESTDIR=$(echo $1 | tr -d '\r')
BUILDDIR="$BASEDIR/builddir/android-aarch64"

if [ -d "$DESTDIR/bin/android/arm64" ]
then
    rm -f $DESTDIR/bin/android/arm64/*.so
else
    mkdir -p $DESTDIR/bin/android/arm64
fi

cp $BUILDDIR/subprojects/**/*.so $DESTDIR/bin/android/arm64

if [ -d "$DESTDIR/maf/swig" ]
then
    rm -f $DESTDIR/maf/swig/*.cs
else
    mkdir -p $DESTDIR/maf/swig
fi

cp $BASEDIR/subprojects/maf_csharp/swig/*.cs $DESTDIR/maf/swig/

# copy FFmpeg dependency to Unity project
if [ -d "$DESTDIR/dependencies/ffmpeg/7.1/android/arm64" ]
then
    rm -f $DESTDIR/dependencies/ffmpeg/7.1/android/arm64/*.so
else
    mkdir -p $DESTDIR/dependencies/ffmpeg/7.1/android/arm64
fi

cp $BASEDIR/subprojects/FFmpeg/arm64-v8a/lib/*.so $DESTDIR/dependencies/ffmpeg/7.1/android/arm64

if [ -d "$DESTDIR/dependencies/libc++/android/arm64" ]
then
    rm -f $DESTDIR/dependencies/libc++/android/arm64/*.so
else
    mkdir -p $DESTDIR/dependencies/libc++/android/arm64
fi

cp $ANDROID_NDK_HOME/sources/cxx-stl/llvm-libc++/libs/arm64-v8a/libc++_shared.so $DESTDIR/dependencies/libc++/android/arm64/libc++_shared.so