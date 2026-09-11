/*
    Custom Media Library Plugin for DeaDBeeF
    Copyright (C) 2026

    Granular cover art display toggles (folder art, track art, or both)
*/

#ifndef __CUSTOM_MEDIALIB_PLUGIN_H__
#define __CUSTOM_MEDIALIB_PLUGIN_H__

#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/artwork.h>
#include <deadbeef/gtkui_api.h>
#include <deadbeef/medialib.h>

#define PLUGIN_ID "custom_medialib"
#define PLUGIN_NAME "Custom Media Library"
#define PLUGIN_VERSION_MAJOR 1
#define PLUGIN_VERSION_MINOR 0

/* Configuration Keys & Defaults */
#define CONF_SHOW_FOLDER_ART "custom_medialib.show_folder_art"
#define CONF_SHOW_TRACK_ART  "custom_medialib.show_track_art"
#define DEFAULT_SHOW_FOLDER_ART 1
#define DEFAULT_SHOW_TRACK_ART  0

/* Global pointers to DeaDBeeF API and sister plugins */
extern DB_functions_t *deadbeef;
extern ddb_artwork_plugin_t *artwork_plugin;
extern ddb_gtkui_t *gtkui_plugin;
extern DB_mediasource_t *medialib_plugin;

/* Configuration accessors */
int plugin_get_show_folder_art(void);
int plugin_get_show_track_art(void);
void plugin_set_show_folder_art(int val);
void plugin_set_show_track_art(int val);

#endif /* __CUSTOM_MEDIALIB_PLUGIN_H__ */
