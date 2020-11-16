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

# make sure to clean up build dir, even if errors occur
cleanup() {
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
}
trap cleanup EXIT

OLD_CWD=$(readlink -f .)

git archive HEAD | tar -C "$BUILD_DIR" -xf -
cd "$BUILD_DIR"

mkdir -p "$BUILD_DIR/AppDir/usr"

# Obtain and compile libncursesw6 so that we get 256 color support
wget http://ftp.gnu.org/gnu/ncurses/ncurses-6.2.tar.gz
tar -xf ncurses-6.2.tar.gz
NCURSES_DIR="$PWD/ncurses-6.2"
pushd "$NCURSES_DIR"
./configure --without-shared --enable-widec --prefix=/ \
    --without-normal --without-debug --without-cxx --without-cxx-binding \
    --without-ada --without-manpages --without-tests
make -j4
make DESTDIR="$PWD/build" install
popd

# Configure vifm to use our libncursesw6
./configure --sysconfdir=/etc --prefix=/usr --with-curses="$NCURSES_DIR/build" \
    --without-gtk --without-X11
make -j4
make DESTDIR="$BUILD_DIR/AppDir" install

# Copy custom AppRun
cp -r "$BUILD_DIR/pkgs/AppImage/AppRun" "$BUILD_DIR/AppDir/"

# Copy the AppData file to AppDir manually
mkdir -p "$BUILD_DIR/AppDir/usr/share/metainfo"
cp -r "$BUILD_DIR/data/vifm.appdata.xml" "$BUILD_DIR/AppDir/usr/share/metainfo"

# Copy terminfo database
cp -r "$NCURSES_DIR/build/share/terminfo" "$BUILD_DIR/AppDir/usr/share"

# Download linuxdeploy

wget --output-document=linuxdeploy \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage

chmod +rx ./linuxdeploy

OUTPUT="vifm-x86_64.AppImage" ./linuxdeploy --appdir ./AppDir --output appimage \
    --desktop-file "$BUILD_DIR/data/vifm.desktop" --icon-file "$BUILD_DIR/data/graphics/vifm.svg" \
    --executable "$BUILD_DIR/AppDir/usr/bin/vifm"

mv "vifm-x86_64.AppImage" "$OLD_CWD"
