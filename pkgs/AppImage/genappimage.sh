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
curl -OL https://invisible-island.net/datafiles/release/ncurses.tar.gz
tar -xf ncurses.tar.gz
NCURSES_DIR="$PWD/ncurses-6.2"
pushd "$NCURSES_DIR"
./configure --with-shared --enable-widec --sysconfdir=/etc --prefix=/usr \
    --without-normal --without-debug
make -j4
make DESTDIR="$BUILD_DIR/AppDir" install
popd

## Configure vifm now to make sure it uses our libncursesw6
export LD_LIBRARY_PATH="$BUILD_DIR/AppDir/usr/lib"
export CPPFLAGS="-I$BUILD_DIR/AppDir/usr/include -I$BUILD_DIR/AppDir/usr/include/ncursesw"
export LDFLAGS="-L$BUILD_DIR/AppDir/usr/lib"
./configure --sysconfdir=/etc --prefix=/usr
make -j4
make DESTDIR="$BUILD_DIR/AppDir" install

# Copy the AppData file to AppDir manually
mkdir -p "$BUILD_DIR/AppDir/usr/share/metainfo"
cp -r "$BUILD_DIR/data/vifm.appdata.xml" "$BUILD_DIR/AppDir/usr/share/metainfo"


# Custom AppRun to avoid $ARGV0 issues when used with zsh
# Reference: https://github.com/neovim/neovim/blob/master/scripts/genappimage.sh
# Reference: https://github.com/neovim/neovim/issues/9341

cd $BUILD_DIR
cat << 'EOF' > AppDir/AppRun
#!/bin/bash
unset ARGV0
export TERMINFO=$APPDIR/usr/share/terminfo
exec "$(dirname "$(readlink -f "${0}")")/usr/bin/vifm" ${@+"$@"}
EOF
chmod 755 AppDir/AppRun


# Downloading linuxdeploy

curl -o ./linuxdeploy -L \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage

chmod +rx ./linuxdeploy

OUTPUT="vifm-x86_64.AppImage" ./linuxdeploy --appdir ./AppDir --output appimage \
    --desktop-file "$BUILD_DIR/data/vifm.desktop" --icon-file "$BUILD_DIR/data/graphics/vifm.png" \
    --executable "$BUILD_DIR/AppDir/usr/bin/vifm" --library "$BUILD_DIR/AppDir/usr/lib/libncursesw.so.6"

mv "vifm-x86_64.AppImage" "$OLD_CWD"
