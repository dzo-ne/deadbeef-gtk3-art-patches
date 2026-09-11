/*
    Custom Media Library Plugin for DeaDBeeF
    Copyright (C) 2026

    Artwork cache subsystem header
*/

#ifndef __CUSTOM_MEDIALIB_ARTWORK_CACHE_H__
#define __CUSTOM_MEDIALIB_ARTWORK_CACHE_H__

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <deadbeef/deadbeef.h>

#define ARTWORK_THUMBNAIL_SIZE 32

typedef void (*artwork_load_callback_t)(GdkPixbuf *pixbuf, gpointer user_data);

/* Subsystem initialization and teardown */
void artwork_cache_init(void);
void artwork_cache_cleanup(void);
void artwork_cache_invalidate(void);
void artwork_cache_cancel_all(void);

/* Default themed fallback icons */
GdkPixbuf *artwork_cache_get_default_folder_icon(void);
GdkPixbuf *artwork_cache_get_default_track_icon(void);

/* Artwork request API */
GdkPixbuf *artwork_cache_request_folder_art(ddb_playItem_t *track,
                                            artwork_load_callback_t callback,
                                            gpointer user_data);

GdkPixbuf *artwork_cache_request_track_art(ddb_playItem_t *track,
                                           artwork_load_callback_t callback,
                                           gpointer user_data);

#endif /* __CUSTOM_MEDIALIB_ARTWORK_CACHE_H__ */
