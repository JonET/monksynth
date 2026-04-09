#!/usr/bin/env bash
#
# Build static (-fPIC) versions of VSTGUI's Linux dependencies.
# Everything is installed into a self-contained prefix so it can be
# pointed at by CMake without polluting the system.
#
# Usage:  ./build-static-deps.sh [prefix]
#         Default prefix: /opt/monksynth-deps

set -euo pipefail

PREFIX="${1:-/opt/monksynth-deps}"
JOBS="$(nproc)"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

export CFLAGS="-fPIC -O2"
export CXXFLAGS="-fPIC -O2"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/lib/x86_64-linux-gnu/pkgconfig:$PREFIX/share/pkgconfig"
export PATH="$PREFIX/bin:$PATH"

# Ensure a recent meson (system packages on Ubuntu 22.04 are too old for
# newer glib/pango/cairo).  We must put /usr/local/bin BEFORE /usr/bin
# so pip3's meson wins over the system package.
pip3 install --break-system-packages meson ninja 2>/dev/null \
    || pip3 install meson ninja
export PATH="/usr/local/bin:$PATH"
hash -r
echo "Using meson: $(command -v meson) version $(meson --version)"

# ---------------------------------------------------------------------------
# Versions
# ---------------------------------------------------------------------------
PCRE2_VER=10.45
LIBFFI_VER=3.4.7
GLIB_VER=2.84.1
GLIB_MAJOR_MINOR=2.84
FRIBIDI_VER=1.0.16
PIXMAN_VER=0.44.2
FREETYPE_VER=2.13.3
FONTCONFIG_VER=2.16.0
HARFBUZZ_VER=10.4.0
CAIRO_VER=1.18.4
PANGO_VER=1.56.1
PANGO_MAJOR_MINOR=1.56
XCB_UTIL_VER=0.4.1
XCB_UTIL_IMAGE_VER=0.4.1
XCB_UTIL_RENDERUTIL_VER=0.3.10
XCB_UTIL_CURSOR_VER=0.1.5
XCB_UTIL_KEYSYMS_VER=0.4.1
XKBCOMMON_VER=1.7.0

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
fetch() {
    local url="$1" dest="$2"
    echo "--- Downloading $url"
    curl -fsSL "$url" -o "$dest"
}

extract() {
    local archive="$1" dir="$2"
    mkdir -p "$dir"
    case "$archive" in
        *.tar.xz)  tar xf "$archive" -C "$dir" --strip-components=1 ;;
        *.tar.gz)  tar xzf "$archive" -C "$dir" --strip-components=1 ;;
        *.tar.bz2) tar xjf "$archive" -C "$dir" --strip-components=1 ;;
    esac
}

build_meson() {
    local srcdir="$1"; shift
    meson setup "$srcdir/_build" "$srcdir" \
        --prefix="$PREFIX" \
        --default-library=static \
        --buildtype=release \
        -Dc_args="-fPIC" \
        -Dcpp_args="-fPIC" \
        "$@"
    ninja -C "$srcdir/_build" -j"$JOBS"
    ninja -C "$srcdir/_build" install
}

build_autotools() {
    local srcdir="$1"; shift
    (cd "$srcdir" && ./configure --prefix="$PREFIX" --enable-static --disable-shared "$@")
    make -C "$srcdir" -j"$JOBS"
    make -C "$srcdir" install
}

build_cmake_lib() {
    local srcdir="$1"; shift
    cmake -S "$srcdir" -B "$srcdir/_build" \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DBUILD_SHARED_LIBS=OFF \
        "$@"
    cmake --build "$srcdir/_build" -j"$JOBS"
    cmake --install "$srcdir/_build"
}

# ---------------------------------------------------------------------------
# 1. pcre2  (glib dependency)
# ---------------------------------------------------------------------------
echo "=== Building pcre2 $PCRE2_VER ==="
fetch "https://github.com/PCRE2Project/pcre2/releases/download/pcre2-$PCRE2_VER/pcre2-$PCRE2_VER.tar.bz2" "$WORKDIR/pcre2.tar.bz2"
extract "$WORKDIR/pcre2.tar.bz2" "$WORKDIR/pcre2"
build_cmake_lib "$WORKDIR/pcre2" \
    -DPCRE2_BUILD_PCRE2GREP=OFF \
    -DPCRE2_BUILD_TESTS=OFF

# ---------------------------------------------------------------------------
# 2. libffi  (glib dependency)
# ---------------------------------------------------------------------------
echo "=== Building libffi $LIBFFI_VER ==="
fetch "https://github.com/libffi/libffi/releases/download/v$LIBFFI_VER/libffi-$LIBFFI_VER.tar.gz" "$WORKDIR/libffi.tar.gz"
extract "$WORKDIR/libffi.tar.gz" "$WORKDIR/libffi"
build_autotools "$WORKDIR/libffi"

# ---------------------------------------------------------------------------
# 3. glib
# ---------------------------------------------------------------------------
echo "=== Building glib $GLIB_VER ==="
fetch "https://download.gnome.org/sources/glib/$GLIB_MAJOR_MINOR/glib-$GLIB_VER.tar.xz" "$WORKDIR/glib.tar.xz"
extract "$WORKDIR/glib.tar.xz" "$WORKDIR/glib"
build_meson "$WORKDIR/glib" \
    -Dtests=false \
    -Dglib_debug=disabled \
    -Dintrospection=disabled

# ---------------------------------------------------------------------------
# 4. fribidi
# ---------------------------------------------------------------------------
echo "=== Building fribidi $FRIBIDI_VER ==="
fetch "https://github.com/fribidi/fribidi/releases/download/v$FRIBIDI_VER/fribidi-$FRIBIDI_VER.tar.xz" "$WORKDIR/fribidi.tar.xz"
extract "$WORKDIR/fribidi.tar.xz" "$WORKDIR/fribidi"
build_meson "$WORKDIR/fribidi" \
    -Dtests=false \
    -Ddocs=false

# ---------------------------------------------------------------------------
# 5. pixman
# ---------------------------------------------------------------------------
echo "=== Building pixman $PIXMAN_VER ==="
fetch "https://www.cairographics.org/releases/pixman-$PIXMAN_VER.tar.gz" "$WORKDIR/pixman.tar.gz"
extract "$WORKDIR/pixman.tar.gz" "$WORKDIR/pixman"
build_meson "$WORKDIR/pixman" \
    -Dtests=disabled \
    -Ddemos=disabled

# ---------------------------------------------------------------------------
# 6. freetype  (first pass, without harfbuzz)
# ---------------------------------------------------------------------------
echo "=== Building freetype $FREETYPE_VER (pass 1, no harfbuzz) ==="
fetch "https://download.savannah.gnu.org/releases/freetype/freetype-$FREETYPE_VER.tar.xz" "$WORKDIR/freetype.tar.xz"
extract "$WORKDIR/freetype.tar.xz" "$WORKDIR/freetype"
build_meson "$WORKDIR/freetype" \
    -Dharfbuzz=disabled \
    -Dpng=enabled \
    -Dzlib=enabled \
    -Dbzip2=enabled \
    -Dbrotli=disabled

# ---------------------------------------------------------------------------
# 7. fontconfig
# ---------------------------------------------------------------------------
echo "=== Building fontconfig $FONTCONFIG_VER ==="
fetch "https://www.freedesktop.org/software/fontconfig/release/fontconfig-$FONTCONFIG_VER.tar.xz" "$WORKDIR/fontconfig.tar.xz"
extract "$WORKDIR/fontconfig.tar.xz" "$WORKDIR/fontconfig"
build_meson "$WORKDIR/fontconfig" \
    -Dtests=disabled \
    -Dtools=disabled \
    -Ddoc=disabled \
    -Dcache-build=disabled

# ---------------------------------------------------------------------------
# 8. harfbuzz
# ---------------------------------------------------------------------------
echo "=== Building harfbuzz $HARFBUZZ_VER ==="
fetch "https://github.com/harfbuzz/harfbuzz/releases/download/$HARFBUZZ_VER/harfbuzz-$HARFBUZZ_VER.tar.xz" "$WORKDIR/harfbuzz.tar.xz"
extract "$WORKDIR/harfbuzz.tar.xz" "$WORKDIR/harfbuzz"
build_meson "$WORKDIR/harfbuzz" \
    -Dglib=enabled \
    -Dfreetype=enabled \
    -Dtests=disabled \
    -Ddocs=disabled \
    -Dintrospection=disabled

# ---------------------------------------------------------------------------
# 9. freetype  (second pass, WITH harfbuzz)
# ---------------------------------------------------------------------------
echo "=== Rebuilding freetype $FREETYPE_VER (pass 2, with harfbuzz) ==="
rm -rf "$WORKDIR/freetype"
extract "$WORKDIR/freetype.tar.xz" "$WORKDIR/freetype"
build_meson "$WORKDIR/freetype" \
    -Dharfbuzz=enabled \
    -Dpng=enabled \
    -Dzlib=enabled \
    -Dbzip2=enabled \
    -Dbrotli=disabled

# ---------------------------------------------------------------------------
# 10. cairo
# ---------------------------------------------------------------------------
echo "=== Building cairo $CAIRO_VER ==="
fetch "https://www.cairographics.org/releases/cairo-$CAIRO_VER.tar.xz" "$WORKDIR/cairo.tar.xz"
extract "$WORKDIR/cairo.tar.xz" "$WORKDIR/cairo"
build_meson "$WORKDIR/cairo" \
    -Dtests=disabled \
    -Dxcb=enabled \
    -Dxlib=enabled \
    -Dpng=enabled \
    -Dfreetype=enabled \
    -Dfontconfig=enabled \
    -Dglib=enabled

# ---------------------------------------------------------------------------
# 11. pango
# ---------------------------------------------------------------------------
echo "=== Building pango $PANGO_VER ==="
fetch "https://download.gnome.org/sources/pango/$PANGO_MAJOR_MINOR/pango-$PANGO_VER.tar.xz" "$WORKDIR/pango.tar.xz"
extract "$WORKDIR/pango.tar.xz" "$WORKDIR/pango"
build_meson "$WORKDIR/pango" \
    -Dintrospection=disabled \
    -Dfontconfig=enabled

# ---------------------------------------------------------------------------
# 12. xcb-util
# ---------------------------------------------------------------------------
echo "=== Building xcb-util $XCB_UTIL_VER ==="
fetch "https://xcb.freedesktop.org/dist/xcb-util-$XCB_UTIL_VER.tar.xz" "$WORKDIR/xcb-util.tar.xz"
extract "$WORKDIR/xcb-util.tar.xz" "$WORKDIR/xcb-util"
build_autotools "$WORKDIR/xcb-util"

# ---------------------------------------------------------------------------
# 13. xcb-util-image  (xcb-util-cursor dependency)
# ---------------------------------------------------------------------------
echo "=== Building xcb-util-image $XCB_UTIL_IMAGE_VER ==="
fetch "https://xcb.freedesktop.org/dist/xcb-util-image-$XCB_UTIL_IMAGE_VER.tar.xz" "$WORKDIR/xcb-util-image.tar.xz"
extract "$WORKDIR/xcb-util-image.tar.xz" "$WORKDIR/xcb-util-image"
build_autotools "$WORKDIR/xcb-util-image"

# ---------------------------------------------------------------------------
# 14. xcb-util-renderutil  (xcb-util-cursor dependency)
# ---------------------------------------------------------------------------
echo "=== Building xcb-util-renderutil $XCB_UTIL_RENDERUTIL_VER ==="
fetch "https://xcb.freedesktop.org/dist/xcb-util-renderutil-$XCB_UTIL_RENDERUTIL_VER.tar.xz" "$WORKDIR/xcb-util-renderutil.tar.xz"
extract "$WORKDIR/xcb-util-renderutil.tar.xz" "$WORKDIR/xcb-util-renderutil"
build_autotools "$WORKDIR/xcb-util-renderutil"

# ---------------------------------------------------------------------------
# 15. xcb-util-cursor
# ---------------------------------------------------------------------------
echo "=== Building xcb-util-cursor $XCB_UTIL_CURSOR_VER ==="
fetch "https://xcb.freedesktop.org/dist/xcb-util-cursor-$XCB_UTIL_CURSOR_VER.tar.xz" "$WORKDIR/xcb-util-cursor.tar.xz"
extract "$WORKDIR/xcb-util-cursor.tar.xz" "$WORKDIR/xcb-util-cursor"
build_autotools "$WORKDIR/xcb-util-cursor"

# ---------------------------------------------------------------------------
# 16. xcb-util-keysyms
# ---------------------------------------------------------------------------
echo "=== Building xcb-util-keysyms $XCB_UTIL_KEYSYMS_VER ==="
fetch "https://xcb.freedesktop.org/dist/xcb-util-keysyms-$XCB_UTIL_KEYSYMS_VER.tar.xz" "$WORKDIR/xcb-util-keysyms.tar.xz"
extract "$WORKDIR/xcb-util-keysyms.tar.xz" "$WORKDIR/xcb-util-keysyms"
build_autotools "$WORKDIR/xcb-util-keysyms"

# ---------------------------------------------------------------------------
# 17. xkbcommon
# ---------------------------------------------------------------------------
echo "=== Building xkbcommon $XKBCOMMON_VER ==="
fetch "https://xkbcommon.org/download/libxkbcommon-$XKBCOMMON_VER.tar.xz" "$WORKDIR/xkbcommon.tar.xz"
extract "$WORKDIR/xkbcommon.tar.xz" "$WORKDIR/xkbcommon"
build_meson "$WORKDIR/xkbcommon" \
    -Denable-docs=false \
    -Denable-tools=false \
    -Denable-wayland=false \
    -Denable-x11=true

echo ""
echo "=== All static dependencies installed to $PREFIX ==="
echo "=== .a files: ==="
find "$PREFIX/lib" -name '*.a' | sort
