#!/bin/bash
# Fetch pinned upstream DeaDBeeF, apply patches/, build the GTK3 UI plugin,
# install it to ~/.local/lib/deadbeef, and delete the temporary checkout.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
UPSTREAM_URL="${UPSTREAM_URL:-https://github.com/DeaDBeeF-Player/deadbeef.git}"
UPSTREAM_REF="${UPSTREAM_REF:-$(grep -v '^#' "$ROOT_DIR/UPSTREAM_REF" | head -n1)}"
OUT_DIR="${OUT_DIR:-$HOME/.local/lib/deadbeef}"
ONLY=""

usage() {
    cat <<U
Usage: $0 [--only medialib|playlist] [--ref <commit|tag>] [--uninstall]
  --only       apply just one feature (default: both)
  --ref        build against a different upstream commit/tag (default: UPSTREAM_REF file)
  --uninstall  remove the installed plugin so the system one is used again
U
}

while [ $# -gt 0 ]; do
    case "$1" in
        --only) ONLY="$2"; shift 2 ;;
        --ref) UPSTREAM_REF="$2"; shift 2 ;;
        --uninstall)
            rm -fv "$OUT_DIR/ddb_gui_GTK3.so"
            exit 0 ;;
        -h|--help) usage; exit 0 ;;
        *) usage; exit 1 ;;
    esac
done

case "$ONLY" in
    "") PATCHES=("$ROOT_DIR"/patches/*.patch) ;;
    medialib) PATCHES=("$ROOT_DIR"/patches/0001-*.patch) ;;
    playlist) PATCHES=("$ROOT_DIR"/patches/0002-*.patch) ;;
    *) usage; exit 1 ;;
esac

for tool in git clang glib-compile-resources pkg-config; do
    command -v "$tool" >/dev/null || { echo "Missing required tool: $tool" >&2; exit 1; }
done
pkg-config --exists gtk+-3.0 || { echo "Missing GTK3 development files (libgtk-3-dev)" >&2; exit 1; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
SRC="$WORK/deadbeef"

echo "==> Fetching upstream $UPSTREAM_REF (shallow)..."
git init -q "$SRC"
git -C "$SRC" fetch -q --depth 1 "$UPSTREAM_URL" "$UPSTREAM_REF"
git -C "$SRC" checkout -q FETCH_HEAD

for p in "${PATCHES[@]}"; do
    echo "==> Applying $(basename "$p")"
    git -C "$SRC" apply --whitespace=nowarn "$p"
done

# Back up whatever was installed before, once.
if [ -f "$OUT_DIR/ddb_gui_GTK3.so" ] && [ ! -f "$OUT_DIR/ddb_gui_GTK3.so.orig" ]; then
    cp "$OUT_DIR/ddb_gui_GTK3.so" "$OUT_DIR/ddb_gui_GTK3.so.orig"
fi

SRC_DIR="$SRC" OUT_DIR="$OUT_DIR" "$SCRIPT_DIR/build_gtk3_ui.sh"
echo "==> Done. Restart DeaDBeeF to load the patched GTK3 UI."
