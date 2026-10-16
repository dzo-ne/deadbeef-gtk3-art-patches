# DeaDBeeF GTK3 UI patches: media library & playlist cover art

Plug-and-play patch set for the DeaDBeeF GTK3 UI plugin. No local upstream checkout needed.

- `patches/0001-medialib-cover-art-toggles.patch`: Preferences → Media Library: `medialib.show_folder_art` (default on), `medialib.show_track_art` (default off).
- `patches/0002-playlist-track-art-column.patch`: "Track Cover Art" playlist column with a self-contained 1000-entry LRU thumbnail cache (covermanager untouched).

Both patches apply independently to upstream.

## Install

```bash
./scripts/install.sh                  # both features
./scripts/install.sh --only medialib  # or: --only playlist
./scripts/install.sh --uninstall      # go back to the stock plugin
```

The script shallow-fetches the upstream commit pinned in `UPSTREAM_REF`, applies the patches, builds only `ddb_gui_GTK3.so`, installs it to `~/.local/lib/deadbeef/`, and deletes the temporary checkout. A previously installed plugin is saved once as `ddb_gui_GTK3.so.orig`.

Requirements: `git`, `clang`, `libgtk-3-dev`, `glib-compile-resources`, and DeaDBeeF in `/opt/deadbeef` (override with `DDB_PREFIX`).

## After a DeaDBeeF update

Change `UPSTREAM_REF` (or pass `--ref <commit|tag>`) to the matching upstream version and rerun. If a patch no longer applies, it needs a rebase.

`scripts/build_gtk3_ui.sh` is the development build against a local clone in `deadbeef-src/` (override with `SRC_DIR`).
