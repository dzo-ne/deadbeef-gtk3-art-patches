#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
SRC_DIR="${SRC_DIR:-$ROOT_DIR/deadbeef-src}"
VENDOR_DIR="$ROOT_DIR/vendor"
OUT_DIR="${OUT_DIR:-$HOME/.local/lib/deadbeef}"
DDB_PREFIX="${DDB_PREFIX:-/opt/deadbeef}"

mkdir -p "$OUT_DIR"

echo "==> Compiling GTKUI resources..."
glib-compile-resources \
    --sourcedir="$SRC_DIR/plugins/gtkui" \
    --target="$SRC_DIR/plugins/gtkui/gtkui-gresources.c" \
    --generate-source "$SRC_DIR/plugins/gtkui/gtkui.gresources.xml"

# Take the GTK UI sources from upstream's Makefile.am (GTKUI_SOURCES, plus the GTK3-only ones),
# so that files added or moved upstream are picked up.
GTKUI_DIR="$SRC_DIR/plugins/gtkui"
mapfile -t GTKUI_SOURCES < <(
    awk '/^GTKUI_SOURCES[[:space:]]*=/ { f = 1 } f { print; if ($0 !~ /\\[[:space:]]*$/) exit }' "$GTKUI_DIR/Makefile.am" \
        | grep -oE '[^[:space:]\\]+\.c' \
        | sed "s|^|$GTKUI_DIR/|"
)
if [ "${#GTKUI_SOURCES[@]}" -eq 0 ]; then
    echo "Could not read GTKUI_SOURCES from $GTKUI_DIR/Makefile.am" >&2
    exit 1
fi

SOURCES=(
    "${GTKUI_SOURCES[@]}"
    "$GTKUI_DIR/deadbeefapp.c"
    "$GTKUI_DIR/gtkui-gresources.c"
    "$SRC_DIR/shared/eqpreset.c"
    "$SRC_DIR/shared/trkproperties_shared.c"
    "$SRC_DIR/shared/analyzer/analyzer.c"
    "$SRC_DIR/shared/scope/scope.c"
    "$SRC_DIR/shared/scriptable/scriptable.c"
    "$SRC_DIR/shared/scriptable/scriptable_model.c"
    "$SRC_DIR/shared/scriptable/scriptable_dsp.c"
    "$SRC_DIR/shared/scriptable/scriptable_encoder.c"
    "$SRC_DIR/shared/deletefromdisk.c"
    "$SRC_DIR/shared/tftintutil.c"
    "$SRC_DIR/shared/growableBuffer.c"
    "$SRC_DIR/shared/pluginsettings.c"
    "$SRC_DIR/shared/parser.c"
)

INCLUDES=(
    -I"$VENDOR_DIR/include"
    -I"$SRC_DIR/include"
    -I"$SRC_DIR/shared"
    -I"$SRC_DIR/plugins/gtkui"
)

CFLAGS=(
    -O2
    -fPIC
    -shared
    -fblocks
    -std=c99
    -D_GNU_SOURCE
    -DVERSION=\"1.10.4-alpha\"
    -DPACKAGE_VERSION=\"1.10.4-alpha\"
    -DDDB_WARN_DEPRECATED=1
    -DGLIB_DISABLE_DEPRECATION_WARNINGS
    -DGDK_DISABLE_DEPRECATION_WARNINGS
    -DGTK_DISABLE_DEPRECATION_WARNINGS
    $(pkg-config --cflags gtk+-3.0)
)

LDFLAGS=(
    -L"$DDB_PREFIX/lib"
    -Wl,-rpath,"$DDB_PREFIX/lib"
    -lBlocksRuntime
    -ldispatch
    -l:libjansson.so.4
    -lm
    $(pkg-config --libs gtk+-3.0)
)

echo "==> Compiling ddb_gui_GTK3.so with clang..."
TMP_SO="$(mktemp --suffix=.so)"
trap 'rm -f "$TMP_SO"' EXIT
clang "${CFLAGS[@]}" "${INCLUDES[@]}" "${SOURCES[@]}" "${LDFLAGS[@]}" -o "$TMP_SO"

# A missing source file only shows up as an unresolved symbol when DeaDBeeF loads the plugin, so check it here,
# before replacing the installed plugin.
UNDEFINED="$(ldd -r "$TMP_SO" 2>&1 | grep 'undefined symbol' || true)"
if [ -n "$UNDEFINED" ]; then
    echo "$UNDEFINED" >&2
    echo "Build produced a plugin with unresolved symbols, not installing it" >&2
    exit 1
fi

install -m 755 "$TMP_SO" "$OUT_DIR/ddb_gui_GTK3.so"
echo "==> Build successful: $OUT_DIR/ddb_gui_GTK3.so"
