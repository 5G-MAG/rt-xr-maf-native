BASEDIR=$(readlink -f $(dirname $0)/..)
DESTDIR=$(echo $1 | tr -d '\r')
BUILDDIR="$BASEDIR/build/android/arm64-v8a"

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

cp $BASEDIR/subprojects/avpipeline/external/avcodec/android/arm64-v8a/lib/*.so $DESTDIR/dependencies/ffmpeg/7.1/android/arm64
cp $BASEDIR/subprojects/avpipeline/external/avcodec/android/arm64-v8a/LICENSE $DESTDIR/dependencies/ffmpeg/7.1/android/arm64

if [ -d "$DESTDIR/dependencies/libc++/android/arm64" ]
then
    rm -f $DESTDIR/dependencies/libc++/android/arm64/*.so
else
    mkdir -p $DESTDIR/dependencies/libc++/android/arm64
fi

llvm_prebuilt="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt"
libcpp_shared="sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"
if [ -d "$llvm_prebuilt/windows-x86_64" ]; then
    cp "$llvm_prebuilt/windows-x86_64/$libcpp_shared" $DESTDIR/dependencies/libc++/android/arm64
elif [ -d "$llvm_prebuilt/darmin-x86_64" ]; then
    cp "$llvm_prebuilt/darmin-x86_64/$libcpp_shared" $DESTDIR/dependencies/libc++/android/arm64
elif [ -d "$llvm_prebuilt/linux-x86_64" ]; then
    cp "$llvm_prebuilt/linux-x86_64/$libcpp_shared" $DESTDIR/dependencies/libc++/android/arm64
fi
