/*
    Custom Media Library Plugin for DeaDBeeF
    Copyright (C) 2026

    Artwork cache subsystem implementation
    - Asynchronous loading via DeaDBeeF artwork API
    - High-performance in-memory thumbnail cache
    - Request deduplication and batch notification
    - Granular toggle enforcement for folders and tracks
*/

#include "artwork_cache.h"
#include "plugin.h"

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>
#include <stdlib.h>

static int64_t s_source_id = 0;
static GMutex s_cache_mutex;
static GHashTable *s_pixbuf_cache = NULL; /* key -> GdkPixbuf* */
static GHashTable *s_in_flight = NULL;     /* key -> GSList of CallbackEntry* */

static GdkPixbuf *s_default_folder_icon = NULL;
static GdkPixbuf *s_default_track_icon = NULL;

typedef struct {
    artwork_load_callback_t callback;
    gpointer user_data;
} CallbackEntry;

typedef struct {
    char *cache_key;
    int size;
} QueryContext;

typedef struct {
    GSList *callbacks; /* list of CallbackEntry */
    GdkPixbuf *pixbuf;
} IdleNotification;

/* Synthesize a clean 1-color placeholder pixbuf if theme icons are unavailable */
static GdkPixbuf *create_fallback_pixbuf(guint32 rgba, int size) {
    GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, size, size);
    if (!pb) {
        return NULL;
    }
    gdk_pixbuf_fill(pb, rgba);
    return pb;
}

static void ensure_default_icons(void) {
    if (!s_default_folder_icon) {
        if (gdk_display_get_default()) {
            GtkIconTheme *theme = gtk_icon_theme_get_default();
            if (theme) {
                s_default_folder_icon = gtk_icon_theme_load_icon(
                    theme, "folder-music", ARTWORK_THUMBNAIL_SIZE, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
                if (!s_default_folder_icon) {
                    s_default_folder_icon = gtk_icon_theme_load_icon(
                        theme, "folder", ARTWORK_THUMBNAIL_SIZE, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
                }
            }
        }
        if (!s_default_folder_icon) {
            /* Fallback: muted amber/brown folder color */
            s_default_folder_icon = create_fallback_pixbuf(0xD9A74AFF, ARTWORK_THUMBNAIL_SIZE);
        }
    }

    if (!s_default_track_icon) {
        if (gdk_display_get_default()) {
            GtkIconTheme *theme = gtk_icon_theme_get_default();
            if (theme) {
                s_default_track_icon = gtk_icon_theme_load_icon(
                    theme, "audio-x-generic", ARTWORK_THUMBNAIL_SIZE, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
                if (!s_default_track_icon) {
                    s_default_track_icon = gtk_icon_theme_load_icon(
                        theme, "media-optical", ARTWORK_THUMBNAIL_SIZE, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
                }
            }
        }
        if (!s_default_track_icon) {
            /* Fallback: muted blue track color */
            s_default_track_icon = create_fallback_pixbuf(0x4A90D9FF, ARTWORK_THUMBNAIL_SIZE);
        }
    }
}

GdkPixbuf *artwork_cache_get_default_folder_icon(void) {
    ensure_default_icons();
    return s_default_folder_icon;
}

GdkPixbuf *artwork_cache_get_default_track_icon(void) {
    ensure_default_icons();
    return s_default_track_icon;
}

void artwork_cache_init(void) {
    g_mutex_init(&s_cache_mutex);
    s_pixbuf_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
    s_in_flight = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    if (!artwork_plugin && deadbeef) {
        artwork_plugin = (ddb_artwork_plugin_t *)deadbeef->plug_get_for_id("artwork2");
        if (!artwork_plugin) {
            artwork_plugin = (ddb_artwork_plugin_t *)deadbeef->plug_get_for_id("artwork");
        }
    }

    if (artwork_plugin && artwork_plugin->allocate_source_id) {
        s_source_id = artwork_plugin->allocate_source_id();
    } else {
        s_source_id = 1;
    }

    ensure_default_icons();
}

static void free_callback_list(GSList *list) {
    for (GSList *l = list; l != NULL; l = l->next) {
        g_free(l->data);
    }
    g_slist_free(list);
}

void artwork_cache_cleanup(void) {
    artwork_cache_cancel_all();

    g_mutex_lock(&s_cache_mutex);
    if (s_in_flight) {
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, s_in_flight);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            free_callback_list((GSList *)value);
        }
        g_hash_table_destroy(s_in_flight);
        s_in_flight = NULL;
    }

    if (s_pixbuf_cache) {
        g_hash_table_destroy(s_pixbuf_cache);
        s_pixbuf_cache = NULL;
    }
    g_mutex_unlock(&s_cache_mutex);
    g_mutex_clear(&s_cache_mutex);

    if (s_default_folder_icon) {
        g_object_unref(s_default_folder_icon);
        s_default_folder_icon = NULL;
    }
    if (s_default_track_icon) {
        g_object_unref(s_default_track_icon);
        s_default_track_icon = NULL;
    }
}

void artwork_cache_cancel_all(void) {
    if (artwork_plugin && artwork_plugin->cancel_queries_with_source_id && s_source_id != 0) {
        artwork_plugin->cancel_queries_with_source_id(s_source_id);
    }
}

void artwork_cache_invalidate(void) {
    g_mutex_lock(&s_cache_mutex);
    if (s_pixbuf_cache) {
        g_hash_table_remove_all(s_pixbuf_cache);
    }
    g_mutex_unlock(&s_cache_mutex);
}

static char *build_track_cache_key(ddb_playItem_t *track) {
    deadbeef->pl_lock();
    const char *uri = deadbeef->pl_find_meta(track, ":URI");
    char *key = NULL;
    if (uri && uri[0]) {
        key = g_strdup_printf("track:%s", uri);
    } else {
        const char *artist = deadbeef->pl_find_meta(track, "artist");
        const char *title = deadbeef->pl_find_meta(track, "title");
        key = g_strdup_printf("track:%s - %s", artist ? artist : "", title ? title : "");
    }
    deadbeef->pl_unlock();
    return key;
}

static char *build_folder_cache_key(ddb_playItem_t *track) {
    deadbeef->pl_lock();
    const char *uri = deadbeef->pl_find_meta(track, ":URI");
    char *key = NULL;
    if (uri && uri[0]) {
        char *dirname = g_path_get_dirname(uri);
        key = g_strdup_printf("folder:%s", dirname);
        g_free(dirname);
    } else {
        const char *artist = deadbeef->pl_find_meta(track, "artist");
        const char *album = deadbeef->pl_find_meta(track, "album");
        key = g_strdup_printf("folder:%s - %s", artist ? artist : "", album ? album : "");
    }
    deadbeef->pl_unlock();
    return key;
}

/* Dispatched to GTK main loop via g_idle_add */
static gboolean on_artwork_idle_dispatch(gpointer data) {
    IdleNotification *notif = (IdleNotification *)data;
    if (!notif) {
        return G_SOURCE_REMOVE;
    }

    for (GSList *l = notif->callbacks; l != NULL; l = l->next) {
        CallbackEntry *entry = (CallbackEntry *)l->data;
        if (entry && entry->callback) {
            entry->callback(notif->pixbuf, entry->user_data);
        }
    }

    free_callback_list(notif->callbacks);
    if (notif->pixbuf) {
        g_object_unref(notif->pixbuf);
    }
    g_free(notif);
    return G_SOURCE_REMOVE;
}

/* Invoked by DeaDBeeF artwork worker thread */
static void on_deadbeef_cover_callback(int error, ddb_cover_query_t *query, ddb_cover_info_t *cover) {
    if (!query) {
        return;
    }

    QueryContext *ctx = (QueryContext *)query->user_data;
    GdkPixbuf *scaled_pb = NULL;

    if (error == 0 && cover && cover->cover_found && cover->image_filename && cover->image_filename[0]) {
        GError *err = NULL;
        scaled_pb = gdk_pixbuf_new_from_file_at_scale(
            cover->image_filename, ctx->size, ctx->size, TRUE, &err);
        if (err) {
            g_error_free(err);
            scaled_pb = NULL;
        }
    }

    g_mutex_lock(&s_cache_mutex);
    if (scaled_pb && s_pixbuf_cache) {
        g_hash_table_insert(s_pixbuf_cache, g_strdup(ctx->cache_key), g_object_ref(scaled_pb));
    }

    GSList *callbacks = NULL;
    if (s_in_flight) {
        callbacks = (GSList *)g_hash_table_lookup(s_in_flight, ctx->cache_key);
        if (callbacks) {
            g_hash_table_remove(s_in_flight, ctx->cache_key);
        }
    }
    g_mutex_unlock(&s_cache_mutex);

    /* Post notification to UI thread */
    if (callbacks) {
        IdleNotification *notif = g_new0(IdleNotification, 1);
        notif->callbacks = callbacks;
        notif->pixbuf = scaled_pb ? g_object_ref(scaled_pb) : NULL;
        g_idle_add(on_artwork_idle_dispatch, notif);
    }

    if (scaled_pb) {
        g_object_unref(scaled_pb);
    }

    /* Release artwork info */
    if (cover && artwork_plugin && artwork_plugin->cover_info_release) {
        artwork_plugin->cover_info_release(cover);
    }

    /* Release playItem reference held for this query */
    if (query->track) {
        deadbeef->pl_item_unref(query->track);
    }

    g_free(ctx->cache_key);
    g_free(ctx);
    free(query);
}

static GdkPixbuf *request_artwork_internal(ddb_playItem_t *track,
                                          const char *cache_key,
                                          int size,
                                          artwork_load_callback_t callback,
                                          gpointer user_data,
                                          GdkPixbuf *default_icon) {
    if (!track || !cache_key) {
        return default_icon;
    }

    g_mutex_lock(&s_cache_mutex);

    /* 1. Check in-memory cache */
    if (s_pixbuf_cache) {
        GdkPixbuf *cached = g_hash_table_lookup(s_pixbuf_cache, cache_key);
        if (cached) {
            g_mutex_unlock(&s_cache_mutex);
            return cached;
        }
    }

    /* 2. Check if already in flight */
    if (s_in_flight && callback) {
        GSList *existing = (GSList *)g_hash_table_lookup(s_in_flight, cache_key);
        if (existing) {
            CallbackEntry *entry = g_new0(CallbackEntry, 1);
            entry->callback = callback;
            entry->user_data = user_data;
            existing = g_slist_append(existing, entry);
            g_hash_table_replace(s_in_flight, g_strdup(cache_key), existing);
            g_mutex_unlock(&s_cache_mutex);
            return default_icon;
        }
    }

    /* 3. Check if artwork plugin is available */
    if (!artwork_plugin || !artwork_plugin->cover_get) {
        g_mutex_unlock(&s_cache_mutex);
        return default_icon;
    }

    /* 4. Register new in-flight query */
    if (callback) {
        CallbackEntry *entry = g_new0(CallbackEntry, 1);
        entry->callback = callback;
        entry->user_data = user_data;
        GSList *callbacks = g_slist_append(NULL, entry);
        g_hash_table_insert(s_in_flight, g_strdup(cache_key), callbacks);
    }
    g_mutex_unlock(&s_cache_mutex);

    /* Allocate and dispatch query */
    QueryContext *ctx = g_new0(QueryContext, 1);
    ctx->cache_key = g_strdup(cache_key);
    ctx->size = size;

    ddb_cover_query_t *query = calloc(1, sizeof(ddb_cover_query_t));
    query->_size = sizeof(ddb_cover_query_t);
    query->track = track;
    deadbeef->pl_item_ref(track);
    query->source_id = s_source_id;
    query->user_data = ctx;

    artwork_plugin->cover_get(query, on_deadbeef_cover_callback);

    return default_icon;
}

GdkPixbuf *artwork_cache_request_folder_art(ddb_playItem_t *track,
                                            artwork_load_callback_t callback,
                                            gpointer user_data) {
    ensure_default_icons();

    /* Granular toggle: If folder cover art is disabled, immediately return stock icon without querying */
    if (!plugin_get_show_folder_art() || !track) {
        return s_default_folder_icon;
    }

    char *key = build_folder_cache_key(track);
    GdkPixbuf *res = request_artwork_internal(
        track, key, ARTWORK_THUMBNAIL_SIZE, callback, user_data, s_default_folder_icon);
    g_free(key);
    return res;
}

GdkPixbuf *artwork_cache_request_track_art(ddb_playItem_t *track,
                                           artwork_load_callback_t callback,
                                           gpointer user_data) {
    ensure_default_icons();

    /* Granular toggle: If track cover art is disabled, immediately return stock icon without querying */
    if (!plugin_get_show_track_art() || !track) {
        return s_default_track_icon;
    }

    char *key = build_track_cache_key(track);
    GdkPixbuf *res = request_artwork_internal(
        track, key, ARTWORK_THUMBNAIL_SIZE, callback, user_data, s_default_track_icon);
    g_free(key);
    return res;
}
