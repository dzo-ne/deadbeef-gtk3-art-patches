/*
    Custom Media Library Plugin for DeaDBeeF
    Copyright (C) 2026

    User Interface declarations (GTK3 TreeView, Layout Widget, and Standalone Window)
*/

#ifndef __CUSTOM_MEDIALIB_UI_H__
#define __CUSTOM_MEDIALIB_UI_H__

#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>
#include <deadbeef/gtkui_api.h>

/* GTKUI layout widget constructor for DeaDBeeF design mode */
ddb_gtkui_widget_t *custom_medialib_widget_create(void);

/* Notifications from plugin / DeaDBeeF events */
void custom_medialib_ui_config_changed(void);
void custom_medialib_ui_refresh(void);
void custom_medialib_ui_cleanup(void);

/* Standalone window support (via View menu / plugin action) */
GtkWidget *custom_medialib_create_window(void);
void custom_medialib_show_window(void);

#endif /* __CUSTOM_MEDIALIB_UI_H__ */
