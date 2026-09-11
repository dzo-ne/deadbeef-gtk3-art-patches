/*
    Custom Media Library Plugin for DeaDBeeF
    Copyright (C) 2026

    Main plugin structure, DeaDBeeF API linkage, preferences mini-DSL,
    and event dispatcher.
*/

#include "plugin.h"
#include "artwork_cache.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exported globals */
DB_functions_t *deadbeef = NULL;
ddb_artwork_plugin_t *artwork_plugin = NULL;
ddb_gtkui_t *gtkui_plugin = NULL;
DB_mediasource_t *medialib_plugin = NULL;

/* Internal configuration state */
static int s_show_folder_art = DEFAULT_SHOW_FOLDER_ART;
static int s_show_track_art = DEFAULT_SHOW_TRACK_ART;

int plugin_get_show_folder_art(void) {
    return s_show_folder_art;
}

int plugin_get_show_track_art(void) {
    return s_show_track_art;
}

void plugin_set_show_folder_art(int val) {
    s_show_folder_art = val;
}

void plugin_set_show_track_art(int val) {
    s_show_track_art = val;
}

/*
    DeaDBeeF Preferences Mini-DSL:
    Exposes two independent checkboxes under:
    Preferences -> Plugins -> Custom Media Library -> Configure
*/
static const char settings_dialog[] =
    "property \"Show cover art on folders/albums\" checkbox " CONF_SHOW_FOLDER_ART " 1;\n"
    "property \"Show cover art on individual tracks\" checkbox " CONF_SHOW_TRACK_ART " 0;\n";

/* Artwork listener callback */
static void on_artwork_settings_did_change(ddb_artwork_listener_event_t event, void *user_data, int64_t p1, int64_t p2) {
    (void)user_data;
    (void)p1;
    (void)p2;
    if (event == DDB_ARTWORK_SETTINGS_DID_CHANGE) {
        artwork_cache_invalidate();
        custom_medialib_ui_config_changed();
    }
}

/* Action callback for View -> Custom Media Library */
#if (DDB_API_LEVEL >= 11)
static int on_action_open_view(DB_plugin_action_t *action, ddb_action_context_t ctx) {
    (void)action;
    (void)ctx;
    custom_medialib_show_window();
    return 0;
}
#elif (DDB_API_LEVEL >= 5)
static int on_action_open_view(DB_plugin_action_t *action, int ctx) {
    (void)action;
    (void)ctx;
    custom_medialib_show_window();
    return 0;
}
#else
static int on_action_open_view(DB_plugin_action_t *action, void *userdata) {
    (void)action;
    (void)userdata;
    custom_medialib_show_window();
    return 0;
}
#endif

static DB_plugin_action_t s_action_open = {
    .title = "View/Custom Media Library",
    .name = "custom_medialib_open_window",
    .flags = DB_ACTION_COMMON,
#if (DDB_API_LEVEL >= 5)
    .callback2 = on_action_open_view,
#else
    .callback = on_action_open_view,
#endif
    .next = NULL
};

static DB_plugin_action_t *plugin_get_actions(DB_playItem_t *it) {
    (void)it;
    return &s_action_open;
}

static int plugin_start(void) {
    /* Read initial preferences */
    s_show_folder_art = deadbeef->conf_get_int(CONF_SHOW_FOLDER_ART, DEFAULT_SHOW_FOLDER_ART);
    s_show_track_art = deadbeef->conf_get_int(CONF_SHOW_TRACK_ART, DEFAULT_SHOW_TRACK_ART);

    artwork_cache_init();
    return 0;
}

static int plugin_connect(void) {
    /* Bind to artwork plugin */
    artwork_plugin = (ddb_artwork_plugin_t *)deadbeef->plug_get_for_id("artwork2");
    if (!artwork_plugin) {
        artwork_plugin = (ddb_artwork_plugin_t *)deadbeef->plug_get_for_id("artwork");
    }

    if (artwork_plugin && artwork_plugin->add_listener) {
        artwork_plugin->add_listener(on_artwork_settings_did_change, NULL);
    }

    /* Bind to GTK UI plugin */
    gtkui_plugin = (ddb_gtkui_t *)deadbeef->plug_get_for_id(DDB_GTKUI_PLUGIN_ID);

    /* Bind to core medialib plugin */
    medialib_plugin = (DB_mediasource_t *)deadbeef->plug_get_for_id("medialib");

    /* Register layout widget for DeaDBeeF Design Mode */
    if (gtkui_plugin && gtkui_plugin->w_reg_widget) {
        gtkui_plugin->w_reg_widget(
            PLUGIN_NAME,
            0,
            custom_medialib_widget_create,
            PLUGIN_ID,
            NULL);
    }

    return 0;
}

static int plugin_disconnect(void) {
    if (gtkui_plugin && gtkui_plugin->w_unreg_widget) {
        gtkui_plugin->w_unreg_widget(PLUGIN_ID);
    }

    if (artwork_plugin && artwork_plugin->remove_listener) {
        artwork_plugin->remove_listener(on_artwork_settings_did_change, NULL);
    }

    custom_medialib_ui_cleanup();
    return 0;
}

static int plugin_stop(void) {
    artwork_cache_cleanup();
    return 0;
}

static int plugin_message(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2) {
    (void)ctx;
    (void)p1;
    (void)p2;

    switch (id) {
    case DB_EV_CONFIGCHANGED: {
        int folder_art = deadbeef->conf_get_int(CONF_SHOW_FOLDER_ART, DEFAULT_SHOW_FOLDER_ART);
        int track_art = deadbeef->conf_get_int(CONF_SHOW_TRACK_ART, DEFAULT_SHOW_TRACK_ART);

        if (folder_art != s_show_folder_art || track_art != s_show_track_art) {
            s_show_folder_art = folder_art;
            s_show_track_art = track_art;
            custom_medialib_ui_config_changed();
        }
        break;
    }

    case DB_EV_PLAYLISTCHANGED:
        custom_medialib_ui_refresh();
        break;

    default:
        break;
    }

    return 0;
}

/* Main plugin descriptor */
static DB_misc_t s_plugin = {
    .plugin.type = DB_PLUGIN_MISC,
    DDB_PLUGIN_SET_API_VERSION
    .plugin.version_major = PLUGIN_VERSION_MAJOR,
    .plugin.version_minor = PLUGIN_VERSION_MINOR,
    .plugin.flags = 0,
    .plugin.id = PLUGIN_ID,
    .plugin.name = PLUGIN_NAME,
    .plugin.descr = "Media Library view with granular folder and track cover art toggles",
    .plugin.copyright = "Copyright (C) 2026 DeaDBeeF Contributors",
    .plugin.website = "https://github.com/DeaDBeeF-Player/deadbeef",
    .plugin.start = plugin_start,
    .plugin.stop = plugin_stop,
    .plugin.connect = plugin_connect,
    .plugin.disconnect = plugin_disconnect,
    .plugin.message = plugin_message,
    .plugin.configdialog = settings_dialog,
    .plugin.get_actions = plugin_get_actions
};

/* Module entry point */
DB_plugin_t *ddb_custom_medialib_load(DB_functions_t *api) {
    deadbeef = api;
    return &s_plugin.plugin;
}

DB_plugin_t *custom_medialib_load(DB_functions_t *api) {
    return ddb_custom_medialib_load(api);
}
