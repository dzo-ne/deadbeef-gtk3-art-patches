/*
    Custom Media Library Plugin for DeaDBeeF
    Copyright (C) 2026

    Media library hierarchy, tree store data model, and playback integration
*/

#ifndef __CUSTOM_MEDIALIB_TREE_H__
#define __CUSTOM_MEDIALIB_TREE_H__

#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>

typedef enum {
    MEDIALIB_NODE_ROOT = 0,
    MEDIALIB_NODE_FOLDER,
    MEDIALIB_NODE_ALBUM,
    MEDIALIB_NODE_TRACK
} MedialibNodeType;

enum {
    COL_PIXBUF = 0,       /* GDK_TYPE_PIXBUF (Folder/Track cover or stock icon) */
    COL_TITLE,            /* G_TYPE_STRING (Folder name, Album title, or Track title) */
    COL_SUBTITLE,         /* G_TYPE_STRING (Artist, track duration, or item count) */
    COL_TRACK,            /* G_TYPE_POINTER (ddb_playItem_t* ref-counted) */
    COL_NODE_TYPE,        /* G_TYPE_INT (MedialibNodeType) */
    COL_IS_LEAF,          /* G_TYPE_BOOLEAN */
    COL_PATH_OR_URI,      /* G_TYPE_STRING */
    MEDIALIB_NUM_COLS
};

/* Store population and lifecycle */
void medialib_tree_populate_store(GtkTreeStore *store, const gchar *filter_text);
void medialib_tree_clear_store(GtkTreeStore *store);
void medialib_tree_update_icons(GtkTreeStore *store, GtkTreeView *tree_view);

/* Playback & playlist actions */
void medialib_tree_play_item(GtkTreeModel *model, GtkTreeIter *iter);
void medialib_tree_add_to_playlist(GtkTreeModel *model, GtkTreeIter *iter, gboolean start_playback, gboolean replace_playlist);

#endif /* __CUSTOM_MEDIALIB_TREE_H__ */
