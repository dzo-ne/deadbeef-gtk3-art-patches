/*
    Custom Media Library Plugin for DeaDBeeF
    Copyright (C) 2026

    Media library hierarchy, tree store population, and playback integration
*/

#include "medialib_tree.h"
#include "artwork_cache.h"
#include "plugin.h"

#include <glib.h>
#include <string.h>

/* Callback when asynchronous artwork loading completes */
static void on_tree_row_art_loaded(GdkPixbuf *pixbuf, gpointer user_data) {
    GtkTreeRowReference *row_ref = (GtkTreeRowReference *)user_data;
    if (!row_ref) {
        return;
    }

    if (gtk_tree_row_reference_valid(row_ref) && pixbuf) {
        GtkTreePath *path = gtk_tree_row_reference_get_path(row_ref);
        GtkTreeModel *model = gtk_tree_row_reference_get_model(row_ref);
        GtkTreeIter iter;
        if (path && model && gtk_tree_model_get_iter(model, &iter, path)) {
            gtk_tree_store_set(GTK_TREE_STORE(model), &iter, COL_PIXBUF, pixbuf, -1);
        }
        if (path) {
            gtk_tree_path_free(path);
        }
    }

    gtk_tree_row_reference_free(row_ref);
}

/* Recursive traversal to find the first playable track in a medialib tree node */
static ddb_playItem_t *find_representative_track(const ddb_medialib_item_t *node) {
    if (!node || !medialib_plugin) {
        return NULL;
    }

    ddb_playItem_t *it = medialib_plugin->tree_item_get_track(node);
    if (it) {
        return it;
    }

    const ddb_medialib_item_t *child = medialib_plugin->tree_item_get_children(node);
    while (child != NULL) {
        it = find_representative_track(child);
        if (it) {
            return it;
        }
        child = medialib_plugin->tree_item_get_next(child);
    }

    return NULL;
}

static void format_duration(float duration, char *buf, size_t sz) {
    int total_sec = (int)(duration + 0.5f);
    int minutes = total_sec / 60;
    int seconds = total_sec % 60;
    snprintf(buf, sz, "%d:%02d", minutes, seconds);
}

static void populate_from_medialib_node(GtkTreeStore *store,
                                       GtkTreeIter *parent,
                                       const ddb_medialib_item_t *node,
                                       const gchar *filter_text) {
    while (node != NULL) {
        const char *text = medialib_plugin->tree_item_get_text(node);
        ddb_playItem_t *track = medialib_plugin->tree_item_get_track(node);
        int children_count = medialib_plugin->tree_item_get_children_count(node);
        gboolean is_leaf = (track != NULL) || (children_count == 0);

        /* Optional filter check */
        if (filter_text && filter_text[0] != '\0') {
            if (!text || !strcasestr(text, filter_text)) {
                /* If this is not matching and has no matching children, skip */
                if (is_leaf) {
                    node = medialib_plugin->tree_item_get_next(node);
                    continue;
                }
            }
        }

        GtkTreeIter iter;
        gtk_tree_store_append(store, &iter, parent);

        GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iter);
        GtkTreeRowReference *row_ref = gtk_tree_row_reference_new(GTK_TREE_MODEL(store), path);
        gtk_tree_path_free(path);

        GdkPixbuf *initial_icon = NULL;
        char subtitle_buf[128] = {0};

        if (track) {
            /* Leaf Track Node */
            deadbeef->pl_item_ref(track);
            deadbeef->pl_lock();
            float duration = deadbeef->pl_get_item_duration(track);
            if (duration > 0.0f) {
                format_duration(duration, subtitle_buf, sizeof(subtitle_buf));
            }
            const char *uri = deadbeef->pl_find_meta(track, ":URI");
            char *uri_dup = uri ? g_strdup(uri) : NULL;
            deadbeef->pl_unlock();

            initial_icon = artwork_cache_request_track_art(track, on_tree_row_art_loaded, row_ref);

            gtk_tree_store_set(store, &iter,
                               COL_PIXBUF, initial_icon,
                               COL_TITLE, text ? text : "Unknown Track",
                               COL_SUBTITLE, subtitle_buf,
                               COL_TRACK, track,
                               COL_NODE_TYPE, MEDIALIB_NODE_TRACK,
                               COL_IS_LEAF, TRUE,
                               COL_PATH_OR_URI, uri_dup,
                               -1);
            g_free(uri_dup);
        } else {
            /* Container / Folder / Album Node */
            ddb_playItem_t *rep_track = find_representative_track(node);
            if (rep_track) {
                deadbeef->pl_item_ref(rep_track);
            }

            if (children_count > 0) {
                snprintf(subtitle_buf, sizeof(subtitle_buf), "%d items", children_count);
            }

            initial_icon = artwork_cache_request_folder_art(rep_track, on_tree_row_art_loaded, row_ref);

            gtk_tree_store_set(store, &iter,
                               COL_PIXBUF, initial_icon,
                               COL_TITLE, text ? text : "Folder",
                               COL_SUBTITLE, subtitle_buf,
                               COL_TRACK, rep_track,
                               COL_NODE_TYPE, MEDIALIB_NODE_FOLDER,
                               COL_IS_LEAF, FALSE,
                               COL_PATH_OR_URI, NULL,
                               -1);

            /* Recurse on children */
            const ddb_medialib_item_t *child = medialib_plugin->tree_item_get_children(node);
            if (child) {
                populate_from_medialib_node(store, &iter, child, filter_text);
            }
        }

        node = medialib_plugin->tree_item_get_next(node);
    }
}

/* Fallback: Populate tree from the current playlist (grouped by Artist -> Album -> Track) */
static void populate_from_playlist_fallback(GtkTreeStore *store, const gchar *filter_text) {
    ddb_playlist_t *plt = deadbeef->plt_get_curr();
    if (!plt) {
        return;
    }

    deadbeef->pl_lock();
    int count = deadbeef->plt_get_item_count(plt, PL_MAIN);
    if (count == 0) {
        deadbeef->pl_unlock();
        deadbeef->plt_unref(plt);
        return;
    }

    /* Map of album_key -> GtkTreeIter for grouping */
    GHashTable *artist_iters = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    GHashTable *album_iters = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    DB_playItem_t *it = deadbeef->plt_get_first(plt, PL_MAIN);
    while (it != NULL) {
        const char *title = deadbeef->pl_find_meta(it, "title");
        const char *artist = deadbeef->pl_find_meta(it, "artist");
        const char *album = deadbeef->pl_find_meta(it, "album");
        const char *uri = deadbeef->pl_find_meta(it, ":URI");
        float duration = deadbeef->pl_get_item_duration(it);

        if (!title && uri) {
            title = g_path_get_basename(uri);
        }

        const char *effective_artist = (artist && artist[0]) ? artist : "Unknown Artist";
        const char *effective_album = (album && album[0]) ? album : "Unknown Album";
        const char *effective_title = (title && title[0]) ? title : "Unknown Title";

        /* Filter text check */
        if (filter_text && filter_text[0] != '\0') {
            if (!strcasestr(effective_title, filter_text) &&
                !strcasestr(effective_artist, filter_text) &&
                !strcasestr(effective_album, filter_text)) {
                DB_playItem_t *next = deadbeef->pl_get_next(it, PL_MAIN);
                deadbeef->pl_item_unref(it);
                it = next;
                continue;
            }
        }

        /* 1. Artist node */
        GtkTreeIter *p_artist_iter = g_hash_table_lookup(artist_iters, effective_artist);
        GtkTreeIter artist_iter;
        if (!p_artist_iter) {
            gtk_tree_store_append(store, &artist_iter, NULL);
            gtk_tree_store_set(store, &artist_iter,
                               COL_PIXBUF, artwork_cache_get_default_folder_icon(),
                               COL_TITLE, effective_artist,
                               COL_SUBTITLE, "Artist",
                               COL_TRACK, NULL,
                               COL_NODE_TYPE, MEDIALIB_NODE_FOLDER,
                               COL_IS_LEAF, FALSE,
                               COL_PATH_OR_URI, NULL,
                               -1);
            p_artist_iter = g_memdup2(&artist_iter, sizeof(GtkTreeIter));
            g_hash_table_insert(artist_iters, g_strdup(effective_artist), p_artist_iter);
        } else {
            artist_iter = *p_artist_iter;
        }

        /* 2. Album node */
        char *album_key = g_strdup_printf("%s||%s", effective_artist, effective_album);
        GtkTreeIter *p_album_iter = g_hash_table_lookup(album_iters, album_key);
        GtkTreeIter album_iter;
        if (!p_album_iter) {
            gtk_tree_store_append(store, &album_iter, &artist_iter);

            GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &album_iter);
            GtkTreeRowReference *row_ref = gtk_tree_row_reference_new(GTK_TREE_MODEL(store), path);
            gtk_tree_path_free(path);

            deadbeef->pl_item_ref(it);
            GdkPixbuf *folder_pb = artwork_cache_request_folder_art(it, on_tree_row_art_loaded, row_ref);

            gtk_tree_store_set(store, &album_iter,
                               COL_PIXBUF, folder_pb,
                               COL_TITLE, effective_album,
                               COL_SUBTITLE, "Album",
                               COL_TRACK, it,
                               COL_NODE_TYPE, MEDIALIB_NODE_ALBUM,
                               COL_IS_LEAF, FALSE,
                               COL_PATH_OR_URI, NULL,
                               -1);

            p_album_iter = g_memdup2(&album_iter, sizeof(GtkTreeIter));
            g_hash_table_insert(album_iters, album_key, p_album_iter);
        } else {
            album_iter = *p_album_iter;
            g_free(album_key);
        }

        /* 3. Track leaf node */
        GtkTreeIter track_iter;
        gtk_tree_store_append(store, &track_iter, &album_iter);

        GtkTreePath *track_path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &track_iter);
        GtkTreeRowReference *track_row_ref = gtk_tree_row_reference_new(GTK_TREE_MODEL(store), track_path);
        gtk_tree_path_free(track_path);

        deadbeef->pl_item_ref(it);
        GdkPixbuf *track_pb = artwork_cache_request_track_art(it, on_tree_row_art_loaded, track_row_ref);

        char dur_buf[32] = {0};
        if (duration > 0.0f) {
            format_duration(duration, dur_buf, sizeof(dur_buf));
        }

        gtk_tree_store_set(store, &track_iter,
                           COL_PIXBUF, track_pb,
                           COL_TITLE, effective_title,
                           COL_SUBTITLE, dur_buf,
                           COL_TRACK, it,
                           COL_NODE_TYPE, MEDIALIB_NODE_TRACK,
                           COL_IS_LEAF, TRUE,
                           COL_PATH_OR_URI, uri,
                           -1);

        DB_playItem_t *next = deadbeef->pl_get_next(it, PL_MAIN);
        deadbeef->pl_item_unref(it);
        it = next;
    }

    g_hash_table_destroy(album_iters);
    g_hash_table_destroy(artist_iters);

    deadbeef->pl_unlock();
    deadbeef->plt_unref(plt);
}

void medialib_tree_populate_store(GtkTreeStore *store, const gchar *filter_text) {
    if (!store) {
        return;
    }

    medialib_tree_clear_store(store);

    gboolean populated = FALSE;

    if (medialib_plugin && medialib_plugin->create_source && medialib_plugin->create_item_tree) {
        ddb_mediasource_source_t *source = medialib_plugin->create_source("custom_medialib");
        if (source) {
            ddb_medialib_item_t *root = medialib_plugin->create_item_tree(source, NULL, filter_text);
            if (root) {
                populate_from_medialib_node(store, NULL, root, filter_text);
                medialib_plugin->free_item_tree(source, root);
                populated = TRUE;
            }
            medialib_plugin->free_source(source);
        }
    }

    /* Fallback if medialib has no configured folders or active scan */
    if (!populated) {
        populate_from_playlist_fallback(store, filter_text);
    }
}

/* Recursively unreference all ddb_playItem_t pointers stored in the model */
static gboolean unref_tracks_traverse(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {
    (void)path;
    (void)data;
    ddb_playItem_t *track = NULL;
    gtk_tree_model_get(model, iter, COL_TRACK, &track, -1);
    if (track) {
        deadbeef->pl_item_unref(track);
    }
    return FALSE; /* continue traversal */
}

void medialib_tree_clear_store(GtkTreeStore *store) {
    if (!store) {
        return;
    }

    gtk_tree_model_foreach(GTK_TREE_MODEL(store), unref_tracks_traverse, NULL);
    gtk_tree_store_clear(store);
}

/* Recursively update icons according to show_folder_art and show_track_art toggles */
static gboolean update_icons_traverse(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data) {
    (void)path;
    (void)data;
    gint node_type = 0;
    gboolean is_leaf = FALSE;
    ddb_playItem_t *track = NULL;

    gtk_tree_model_get(model, iter,
                       COL_NODE_TYPE, &node_type,
                       COL_IS_LEAF, &is_leaf,
                       COL_TRACK, &track,
                       -1);

    if (is_leaf || node_type == MEDIALIB_NODE_TRACK) {
        /* Track Node */
        if (!plugin_get_show_track_art()) {
            gtk_tree_store_set(GTK_TREE_STORE(model), iter,
                               COL_PIXBUF, artwork_cache_get_default_track_icon(), -1);
        } else if (track) {
            GtkTreeRowReference *ref = gtk_tree_row_reference_new(model, path);
            GdkPixbuf *pb = artwork_cache_request_track_art(track, on_tree_row_art_loaded, ref);
            gtk_tree_store_set(GTK_TREE_STORE(model), iter, COL_PIXBUF, pb, -1);
        }
    } else {
        /* Folder / Album Node */
        if (!plugin_get_show_folder_art()) {
            gtk_tree_store_set(GTK_TREE_STORE(model), iter,
                               COL_PIXBUF, artwork_cache_get_default_folder_icon(), -1);
        } else if (track) {
            GtkTreeRowReference *ref = gtk_tree_row_reference_new(model, path);
            GdkPixbuf *pb = artwork_cache_request_folder_art(track, on_tree_row_art_loaded, ref);
            gtk_tree_store_set(GTK_TREE_STORE(model), iter, COL_PIXBUF, pb, -1);
        }
    }

    return FALSE;
}

void medialib_tree_update_icons(GtkTreeStore *store, GtkTreeView *tree_view) {
    if (!store) {
        return;
    }

    gtk_tree_model_foreach(GTK_TREE_MODEL(store), update_icons_traverse, NULL);

    if (tree_view) {
        gtk_widget_queue_draw(GTK_WIDGET(tree_view));
    }
}

/* Collect all tracks under a tree node */
static void collect_tracks_recursive(GtkTreeModel *model, GtkTreeIter *parent, GList **out_list) {
    GtkTreeIter iter;
    if (!gtk_tree_model_iter_children(model, &iter, parent)) {
        return;
    }

    do {
        gboolean is_leaf = FALSE;
        ddb_playItem_t *track = NULL;
        gtk_tree_model_get(model, &iter, COL_IS_LEAF, &is_leaf, COL_TRACK, &track, -1);

        if (is_leaf && track) {
            deadbeef->pl_item_ref(track);
            *out_list = g_list_append(*out_list, track);
        } else {
            collect_tracks_recursive(model, &iter, out_list);
        }
    } while (gtk_tree_model_iter_next(model, &iter));
}

void medialib_tree_add_to_playlist(GtkTreeModel *model, GtkTreeIter *iter, gboolean start_playback, gboolean replace_playlist) {
    if (!model || !iter) {
        return;
    }

    GList *tracks = NULL;
    gboolean is_leaf = FALSE;
    ddb_playItem_t *track = NULL;

    gtk_tree_model_get(model, iter, COL_IS_LEAF, &is_leaf, COL_TRACK, &track, -1);

    if (is_leaf && track) {
        deadbeef->pl_item_ref(track);
        tracks = g_list_append(tracks, track);
    } else {
        collect_tracks_recursive(model, iter, &tracks);
    }

    if (!tracks) {
        return;
    }

    ddb_playlist_t *plt = deadbeef->plt_get_curr();
    if (!plt) {
        int idx = deadbeef->plt_add(-1, "Default");
        deadbeef->plt_set_curr_idx(idx);
        plt = deadbeef->plt_get_curr();
    }

    if (!plt) {
        for (GList *l = tracks; l != NULL; l = l->next) {
            deadbeef->pl_item_unref((ddb_playItem_t *)l->data);
        }
        g_list_free(tracks);
        return;
    }

    deadbeef->pl_lock();
    if (replace_playlist) {
        deadbeef->plt_clear(plt);
    }

    DB_playItem_t *first_added = NULL;
    DB_playItem_t *tail = deadbeef->plt_get_last(plt, PL_MAIN);

    for (GList *l = tracks; l != NULL; l = l->next) {
        ddb_playItem_t *t = (ddb_playItem_t *)l->data;
        DB_playItem_t *inserted = deadbeef->plt_insert_item(plt, tail, t);
        if (inserted) {
            if (!first_added) {
                first_added = inserted;
                deadbeef->pl_item_ref(first_added);
            }
            if (tail) {
                deadbeef->pl_item_unref(tail);
            }
            tail = inserted;
            deadbeef->pl_item_ref(tail);
        }
        deadbeef->pl_item_unref(t);
    }

    if (tail) {
        deadbeef->pl_item_unref(tail);
    }

    deadbeef->plt_modified(plt);
    deadbeef->pl_unlock();

    deadbeef->sendmessage(DB_EV_PLAYLISTCHANGED, 0, DDB_PLAYLIST_CHANGE_CONTENT, 0);

    if (start_playback && first_added) {
        int track_idx = deadbeef->plt_get_item_idx(plt, first_added, PL_MAIN);
        if (track_idx >= 0) {
            deadbeef->sendmessage(DB_EV_PLAY_NUM, 0, track_idx, 0);
        }
    }

    if (first_added) {
        deadbeef->pl_item_unref(first_added);
    }

    deadbeef->plt_unref(plt);
    g_list_free(tracks);
}

void medialib_tree_play_item(GtkTreeModel *model, GtkTreeIter *iter) {
    medialib_tree_add_to_playlist(model, iter, TRUE, TRUE);
}
