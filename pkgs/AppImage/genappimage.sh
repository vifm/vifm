#!/bin/bash
# Contributed by michaellee8 <ckmichael8@gmail.com> in 2020

set -x
set -e

# Reference: https://docs.appimage.org/packaging-guide/from-source/native-binaries.html#id2

# building in temporary directory to keep system clean
# use RAM disk if possible (as in: not building on CI system
# like Travis, and RAM disk is available)
# DISABLED: It seems that linuxdeploy won't be executable on shared memory,
# maybe /dev/shm is marked as non-executable?
if [ "$CI" == "" ] && [ -d /dev/shm ] && false; then
    TEMP_BASE=/dev/shm
else
    TEMP_BASE=/tmp
fi

BUILD_DIR=$(mktemp -d -p "$TEMP_BASE" appimage-build-XXXXXX)
VERSION=$(git describe | sed 's/-g.*$//')

# make sure to clean up build dir, even if errors occur
cleanup() {
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
}
trap cleanup EXIT

OLD_CWD=$(readlink -f .)

cd "$(git rev-parse --show-toplevel)"
git archive HEAD | tar -C "$BUILD_DIR" -xf -
cd "$BUILD_DIR"

mkdir -p "$BUILD_DIR/AppDir/usr"

# Obtain and compile libncursesw6 so that we get 256 color support
wget http://ftp.gnu.org/gnu/ncurses/ncurses-6.4.tar.gz
tar -xf ncurses-6.4.tar.gz
NCURSES_DIR="$PWD/ncurses-6.4"
pushd "$NCURSES_DIR"
./configure --without-shared --enable-widec --prefix=/ \
    --without-normal --without-debug --without-cxx --without-cxx-binding \
    --without-ada --without-manpages --without-tests --without-gpm
make -j4
make DESTDIR="$PWD/build" install
popd

# Configure vifm to use our libncursesw6
./configure --sysconfdir=/etc --prefix=/usr --with-curses="$NCURSES_DIR/build" \
    --without-gtk --without-X11 --without-libmagic
make -j4
make DESTDIR="$BUILD_DIR/AppDir" install

# Setup root files
cp "$BUILD_DIR/pkgs/AppImage/AppRun" "$BUILD_DIR/AppDir/"
cp "$BUILD_DIR/data/graphics/vifm.svg" "$BUILD_DIR/AppDir/"
ln -s usr/share/applications/vifm.desktop "$BUILD_DIR/AppDir/"

# Copy the AppData file to AppDir manually
mkdir -p "$BUILD_DIR/AppDir/usr/share/metainfo"
cp "$BUILD_DIR/data/vifm.appdata.xml" "$BUILD_DIR/AppDir/usr/share/metainfo"

# Copy terminfo database
cp -r "$NCURSES_DIR/build/share/terminfo" "$BUILD_DIR/AppDir/usr/share"

# Prepare the binary
strip "$BUILD_DIR/AppDir/usr/bin/vifm"
mkdir -p "$BUILD_DIR/AppDir/usr/lib"
ldd "$BUILD_DIR/AppDir/usr/bin/vifm" | grep '=> /' | cut -d' ' -f3 |
    grep -v -e '/libc\.' -e '/libm\.' -e '/libpthread\.' -e '/librt\.' \
            -e '/libdl\.' -e '/libz\.' -e '/libresolv\.' |
    xargs -I {} cp -f {} "$BUILD_DIR/AppDir/usr/lib"

# Make AppImage
cd "$OLD_CWD"
if gpg --list-secret-keys xaizek@posteo.net > /dev/null; then
    appimagetool --sign "$BUILD_DIR/AppDir" "vifm-$VERSION-x86_64.AppImage" \
                 --updateinformation 'gh-releases-zsync|vifm|vifm|latest|vifm-*x86_64.AppImage.zsync'
else
    appimagetool "$BUILD_DIR/AppDir" "vifm-$VERSION-x86_64.AppImage"
fi
