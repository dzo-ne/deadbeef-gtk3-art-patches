# DeaDBeeF Custom Media Library Plugin (`ddb_custom_medialib`)

A standalone, out-of-tree plugin for the [DeaDBeeF music player](https://github.com/DeaDBeeF-Player/deadbeef) providing a custom Media Library tree view with **granular cover art display toggles** (folder/album art, individual track art, or both).

Architected with clean modular separation and memory safety so it can be ported upstream directly into DeaDBeeF core.

---

## Features

- **Granular Artwork Toggles**:
  - `custom_medialib.show_folder_art` (default: `1`): Toggle cover art display on folders and album containers.
  - `custom_medialib.show_track_art` (default: `0`): Toggle cover art display on individual track leaf items.
  - Integrated into DeaDBeeF preferences dialog (`Preferences -> Plugins -> Custom Media Library -> Configure`) via DeaDBeeF's `configdialog` mini-DSL.
  - Live redraws on `DB_EV_CONFIGCHANGED` without restarting player or reloading the library.
- **Asynchronous Artwork Fetching**:
  - Leverages DeaDBeeF's `ddb_artwork_plugin_t` subsystem.
  - In-memory scaled thumbnail cache (`GHashTable` with reference counting) avoiding repeated disk I/O and decodes.
  - Request deduplication and batch notification for high efficiency.
  - Instant UI responsiveness: background scaling with main loop dispatch via `g_idle_add`.
  - Themed fallback stock icons (`folder-music` / `audio-x-generic`) when artwork is toggled off or unavailable.
- **Full Library & Playlist Hierarchy**:
  - Connects to DeaDBeeF's `DB_mediasource_t` (`medialib`) API.
  - Automatic fallback grouping (Artist &rarr; Album &rarr; Tracks) from active playlist if no media library folders are configured yet.
- **UI Integration**:
  - **DeaDBeeF Design Mode**: Registerable layout widget (`custom_medialib`) embeddable into tabs, splitters, and panels.
  - **Menu Action / Standalone Window**: Accessible directly via `View -> Custom Media Library`.
  - Real-time search/filter entry with debounced query evaluation.
  - Double-click and right-click context actions (`Play`, `Add to Playlist`, `Expand/Collapse All`, quick cover art toggles).

---

## Directory Structure

```
deadbeef-medialib-art/
├── CMakeLists.txt              # CMake build configuration with CTest suite
├── Makefile                    # Standalone out-of-tree Makefile
├── README.md                   # Build and usage documentation
├── include/
│   ├── artwork_cache.h         # Artwork loader, deduplication, thumbnail cache
│   ├── medialib_tree.h         # Tree store model, hierarchy traversal, playback
│   ├── plugin.h                # Plugin definitions, config keys, sister plugins
│   └── ui.h                    # GTK3 TreeView, design mode widget, standalone window
├── src/
│   ├── artwork_cache.c         # Artwork subsystem integration & GdkPixbuf cache
│   ├── medialib_tree.c         # GtkTreeStore population & DeaDBeeF playback hooks
│   ├── plugin.c                # DB_misc_t entry point, configdialog DSL, event bus
│   └── ui.c                    # GTK3 view rendering, search filter, context menus
└── tests/
    └── test_plugin.c           # Automated verification test suite (dlopen, ABI, config)
```

---

## Dependencies

- **GTK+ 3.0** development headers (`libgtk-3-dev`)
- **DeaDBeeF development headers** (`deadbeef/deadbeef.h`, `artwork.h`, `gtkui_api.h`, `medialib.h`)
  - Automatically detected from `/opt/deadbeef/include`, `/usr/include`, `/usr/local/include`, or customizable with `-DDEADBEEF_INCLUDE_DIR`
- **Build tools**: `pkg-config`, `gcc` or `clang`, `cmake` (>= 3.10) or `make`

---

## Building and Installation

### Option 1: Using CMake (Recommended)

1. Create a build directory and configure:
   ```bash
   mkdir build && cd build
   cmake ..
   ```
   *(If your DeaDBeeF headers are in a custom path: `cmake -DDEADBEEF_INCLUDE_DIR=/path/to/include ..`)*

2. Build and run automated tests:
   ```bash
   make
   ctest --output-on-failure
   ```

3. Install:
   - **User installation** (installs to `~/.local/lib/deadbeef/`):
     ```bash
     make install-user
     ```
   - **System installation** (installs to `/usr/local/lib/deadbeef/`):
     ```bash
     sudo make install
     ```

### Option 2: Using Makefile

1. Build the shared library:
   ```bash
   make
   ```

2. Run automated test suite:
   ```bash
   make test
   ```

3. Install:
   - **User installation**:
     ```bash
     make install-user
     ```
   - **System installation**:
     ```bash
     sudo make install
     ```

---

## Configuration

In DeaDBeeF, open **Edit &rarr; Preferences &rarr; Plugins &rarr; Custom Media Library &rarr; Configure**:

| Option | Configuration Key | Default | Description |
| :--- | :--- | :---: | :--- |
| **Show cover art on folders/albums** | `custom_medialib.show_folder_art` | `1` (Enabled) | Renders cover art thumbnails for folders/albums. When disabled, displays the standard folder stock icon without querying the artwork backend. |
| **Show cover art on individual tracks** | `custom_medialib.show_track_art` | `0` (Disabled) | Renders cover art thumbnails for individual track rows. When disabled, displays standard audio track icons without querying the artwork backend. |

Changes take effect immediately across all active views via `DB_EV_CONFIGCHANGED`. You can also toggle these settings on the fly by right-clicking inside the Media Library view.

---

## Using the Plugin

1. **In DeaDBeeF Layout (Design Mode)**:
   - Go to `View` &rarr; `Design mode`.
   - Right-click on any widget or splitter where you want the media library.
   - Choose `Add` or `Replace with` &rarr; `Custom Media Library`.
   - Turn off `Design mode`.

2. **As a Standalone Window**:
   - Go to `View` &rarr; `Custom Media Library`.
   - A dedicated window opens showing your entire media library tree with cover artwork according to your preferences.

3. **Interacting with Tracks**:
   - **Double-click** or press **Enter** on a track or folder: Replaces the current playlist and starts playback.
   - **Right-click**:
     - `Play`: Replaces current playlist and plays selected item/folder.
     - `Add to Playlist`: Appends selected item/folder to active playlist.
     - `Expand All` / `Collapse All`: Adjusts tree expansion.
     - Quick toggles for folder art and track art.
     - `Refresh Library`: Clears caches and re-reads media library items.

---

## Porting Upstream to DeaDBeeF Core

The codebase follows DeaDBeeF conventions and can be easily ported into upstream DeaDBeeF:
- `src/artwork_cache.c` and `include/artwork_cache.h`: General-purpose GTK artwork thumbnailing and deduplication cache.
- `src/medialib_tree.c` and `include/medialib_tree.h`: Adapts `DB_mediasource_t` tree items into a `GtkTreeStore` model.
- `src/ui.c` and `include/ui.h`: Implements standard `ddb_gtkui_widget_t` layout widget.
- Memory management: All `ddb_playItem_t` objects use `deadbeef->pl_item_ref` / `deadbeef->pl_item_unref`; GObject pixbufs and row references are strictly freed to prevent memory leaks during scrolling or reloads.
