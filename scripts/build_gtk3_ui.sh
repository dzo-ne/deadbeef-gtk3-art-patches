#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
SRC_DIR="$ROOT_DIR/deadbeef-src"
DEPS_DIR="$ROOT_DIR/deps"
OUT_DIR="$HOME/.local/lib/deadbeef"

mkdir -p "$OUT_DIR"

echo "==> Compiling GTKUI resources..."
glib-compile-resources \
    --sourcedir="$SRC_DIR/plugins/gtkui" \
    --target="$SRC_DIR/plugins/gtkui/gtkui-gresources.c" \
    --generate-source "$SRC_DIR/plugins/gtkui/gtkui.gresources.xml"

SOURCES=(
    "$SRC_DIR/plugins/gtkui"/*.c
    "$SRC_DIR/plugins/gtkui/medialib"/*.c
    "$SRC_DIR/plugins/gtkui/prefwin"/*.c
    "$SRC_DIR/plugins/gtkui/covermanager"/*.c
    "$SRC_DIR/plugins/gtkui/playlist"/*.c
    "$SRC_DIR/plugins/gtkui/scriptable"/*.c
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
    "$SRC_DIR/src/utf8.c"
)

INCLUDES=(
    -I"$DEPS_DIR/include"
    -I"$DEPS_DIR/jansson/usr/include"
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
    -L"$DEPS_DIR/lib"
    -L/opt/deadbeef/lib
    -Wl,-rpath,/opt/deadbeef/lib
    -lBlocksRuntime
    -ldispatch
    -ljansson
    -lm
    $(pkg-config --libs gtk+-3.0)
)

echo "==> Compiling ddb_gui_GTK3.so with clang..."
clang "${CFLAGS[@]}" "${INCLUDES[@]}" "${SOURCES[@]}" "${LDFLAGS[@]}" -o "$OUT_DIR/ddb_gui_GTK3.so"

echo "==> Build successful: $OUT_DIR/ddb_gui_GTK3.so"
