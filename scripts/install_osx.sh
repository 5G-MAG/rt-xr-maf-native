BASEDIR=$(readlink -f $(dirname $0)/..)
DESTDIR=$(echo $1 | tr -d '\r')
BUILDDIR="$BASEDIR/builddir/osx"

if [ -d "$DESTDIR/bin/osx/arm64" ]
then
    rm -f $DESTDIR/bin/osx/arm64/*.dylib
else
    mkdir -p $DESTDIR/bin/osx/arm64
fi

cp $BUILDDIR/subprojects/**/*.dylib $DESTDIR/bin/osx/arm64

if [ -d "$DESTDIR/maf/swig" ]
then
    rm -f $DESTDIR/maf/swig/*.cs
else
    mkdir -p $DESTDIR/maf/swig
fi

cp $BASEDIR/subprojects/maf_csharp/swig/*.cs $DESTDIR/maf/swig/

# the rt-xr-maf-native commit the libraries and bindings are built from
if [ -f "$BUILDDIR/rt-xr-maf-native.version" ]
then
    cp $BUILDDIR/rt-xr-maf-native.version $DESTDIR/
else
    echo "warning: $BUILDDIR/rt-xr-maf-native.version not found, $DESTDIR/rt-xr-maf-native.version is not updated" >&2
fi

# copy FFmpeg dependency to Unity project
if [ -d "$DESTDIR/dependencies/ffmpeg/7.1/osx/arm64" ]
then
    rm -f $DESTDIR/dependencies/ffmpeg/7.1/osx/arm64/*.dylib
else
    mkdir -p $DESTDIR/dependencies/ffmpeg/7.1/osx/arm64
fi

AVCODEC_LIB="$BASEDIR/subprojects/avpipeline/external/avcodec/osx/lib"
FFMPEG_DEST="$DESTDIR/dependencies/ffmpeg/7.1/osx/arm64"

# copy each library once, under the major version name the binaries link against (eg. libavcodec.61.dylib)
for lib in $AVCODEC_LIB/lib*.dylib
do
    name=$(basename $lib)
    if [[ "$name" =~ ^lib[a-z]+\.[0-9]+\.dylib$ ]]
    then
        cp -L $lib $FFMPEG_DEST/$name
    fi
done
# cp $BASEDIR/subprojects/avpipeline/external/avcodec/osx/LICENSE $DESTDIR/dependencies/ffmpeg/7.1/osx/arm64

# make the libraries relocatable: ffmpeg's install names are absolute paths into $AVCODEC_LIB,
# and the meson build rpaths point into $BUILDDIR, neither exists outside of this repository
relink() {
    for dep in $(otool -L $1 | tail -n +2 | awk '{print $1}' | grep "^$AVCODEC_LIB/")
    do
        install_name_tool -change $dep $2/$(basename $dep) $1
    done
}

for lib in $FFMPEG_DEST/*.dylib
do
    install_name_tool -id @rpath/$(basename $lib) $lib
    relink $lib @loader_path
done

for lib in $DESTDIR/bin/osx/arm64/*.dylib
do
    for rpath in $(otool -l $lib | grep -A2 LC_RPATH | awk '/ path /{print $2}')
    do
        install_name_tool -delete_rpath $rpath $lib
    done
    install_name_tool -add_rpath @loader_path -add_rpath @loader_path/../../../dependencies/ffmpeg/7.1/osx/arm64 $lib
    relink $lib @rpath
done

# install_name_tool invalidates the signature, arm64 refuses to load unsigned code
codesign --force --sign - $DESTDIR/bin/osx/arm64/*.dylib $FFMPEG_DEST/*.dylib

