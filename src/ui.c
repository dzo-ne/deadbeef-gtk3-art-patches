/*
    Custom Media Library Plugin for DeaDBeeF
    Copyright (C) 2026

    User Interface implementation (GTK3 TreeView, Layout Widget, and Standalone Window)
*/

#include "ui.h"
#include "medialib_tree.h"
#include "artwork_cache.h"
#include "plugin.h"

#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    GtkWidget *container;
    GtkWidget *search_entry;
    GtkWidget *refresh_btn;
    GtkWidget *status_label;
    GtkWidget *scrolled_win;
    GtkWidget *tree_view;
    GtkTreeStore *store;
    guint search_timeout_id;
    ddb_gtkui_widget_t *gtkui_w;
} MedialibView;

static GSList *s_active_views = NULL;
static GtkWidget *s_standalone_window = NULL;

static void view_update_status(MedialibView *view) {
    if (!view || !view->status_label || !view->store) {
        return;
    }

    int count = 0;
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(view->store), &iter)) {
        do {
            count++;
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(view->store), &iter));
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "%d top-level item%s", count, count == 1 ? "" : "s");
    gtk_label_set_text(GTK_LABEL(view->status_label), buf);
}

static void on_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    (void)column;
    (void)user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        medialib_tree_play_item(model, &iter);
    }
}

static void on_menu_play_activate(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    MedialibView *view = (MedialibView *)user_data;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view->tree_view));
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        medialib_tree_play_item(model, &iter);
    }
}

static void on_menu_enqueue_activate(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    MedialibView *view = (MedialibView *)user_data;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view->tree_view));
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        medialib_tree_add_to_playlist(model, &iter, FALSE, FALSE);
    }
}

static void on_menu_toggle_folder_art(GtkCheckMenuItem *item, gpointer user_data) {
    (void)user_data;
    gboolean active = gtk_check_menu_item_get_active(item);
    plugin_set_show_folder_art(active ? 1 : 0);
    deadbeef->conf_set_int(CONF_SHOW_FOLDER_ART, active ? 1 : 0);
    deadbeef->conf_save();
    deadbeef->sendmessage(DB_EV_CONFIGCHANGED, 0, 0, 0);
}

static void on_menu_toggle_track_art(GtkCheckMenuItem *item, gpointer user_data) {
    (void)user_data;
    gboolean active = gtk_check_menu_item_get_active(item);
    plugin_set_show_track_art(active ? 1 : 0);
    deadbeef->conf_set_int(CONF_SHOW_TRACK_ART, active ? 1 : 0);
    deadbeef->conf_save();
    deadbeef->sendmessage(DB_EV_CONFIGCHANGED, 0, 0, 0);
}

static void on_menu_expand_all(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    MedialibView *view = (MedialibView *)user_data;
    gtk_tree_view_expand_all(GTK_TREE_VIEW(view->tree_view));
}

static void on_menu_collapse_all(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    MedialibView *view = (MedialibView *)user_data;
    gtk_tree_view_collapse_all(GTK_TREE_VIEW(view->tree_view));
}

static void on_menu_refresh(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    (void)user_data;
    custom_medialib_ui_refresh();
}

static void show_context_menu(MedialibView *view, GdkEventButton *event) {
    GtkWidget *menu = gtk_menu_new();

    GtkWidget *item_play = gtk_menu_item_new_with_mnemonic("_Play");
    g_signal_connect(item_play, "activate", G_CALLBACK(on_menu_play_activate), view);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_play);

    GtkWidget *item_enq = gtk_menu_item_new_with_mnemonic("_Add to Playlist");
    g_signal_connect(item_enq, "activate", G_CALLBACK(on_menu_enqueue_activate), view);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_enq);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    GtkWidget *item_exp = gtk_menu_item_new_with_label("Expand All");
    g_signal_connect(item_exp, "activate", G_CALLBACK(on_menu_expand_all), view);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_exp);

    GtkWidget *item_col = gtk_menu_item_new_with_label("Collapse All");
    g_signal_connect(item_col, "activate", G_CALLBACK(on_menu_collapse_all), view);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_col);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    GtkWidget *item_folder_art = gtk_check_menu_item_new_with_label("Show folder cover art");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_folder_art), plugin_get_show_folder_art());
    g_signal_connect(item_folder_art, "toggled", G_CALLBACK(on_menu_toggle_folder_art), view);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_folder_art);

    GtkWidget *item_track_art = gtk_check_menu_item_new_with_label("Show track cover art");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item_track_art), plugin_get_show_track_art());
    g_signal_connect(item_track_art, "toggled", G_CALLBACK(on_menu_toggle_track_art), view);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_track_art);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    GtkWidget *item_ref = gtk_menu_item_new_with_label("Refresh Library");
    g_signal_connect(item_ref, "activate", G_CALLBACK(on_menu_refresh), view);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_ref);

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
}

static gboolean on_tree_view_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    MedialibView *view = (MedialibView *)user_data;

    if (gdk_event_triggers_context_menu((GdkEvent *)event)) {
        GtkTreePath *path = NULL;
        if (gtk_tree_view_get_path_at_pos(
                GTK_TREE_VIEW(widget), (gint)event->x, (gint)event->y, &path, NULL, NULL, NULL)) {
            GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
            gtk_tree_selection_select_path(sel, path);
            gtk_tree_path_free(path);
        }
        show_context_menu(view, event);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_search_timeout(gpointer user_data) {
    MedialibView *view = (MedialibView *)user_data;
    view->search_timeout_id = 0;

    const gchar *text = gtk_entry_get_text(GTK_ENTRY(view->search_entry));
    medialib_tree_populate_store(view->store, text);
    view_update_status(view);

    if (text && text[0] != '\0') {
        gtk_tree_view_expand_all(GTK_TREE_VIEW(view->tree_view));
    }

    return G_SOURCE_REMOVE;
}

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    (void)entry;
    MedialibView *view = (MedialibView *)user_data;
    if (view->search_timeout_id != 0) {
        g_source_remove(view->search_timeout_id);
    }
    view->search_timeout_id = g_timeout_add(250, on_search_timeout, view);
}

static void on_refresh_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    MedialibView *view = (MedialibView *)user_data;
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(view->search_entry));
    artwork_cache_invalidate();
    medialib_tree_populate_store(view->store, text);
    view_update_status(view);
}

static void on_view_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    MedialibView *view = (MedialibView *)user_data;

    s_active_views = g_slist_remove(s_active_views, view);

    if (view->search_timeout_id != 0) {
        g_source_remove(view->search_timeout_id);
        view->search_timeout_id = 0;
    }

    if (view->store) {
        medialib_tree_clear_store(view->store);
        g_object_unref(view->store);
        view->store = NULL;
    }

    g_free(view);
}

static MedialibView *create_medialib_view(void) {
    MedialibView *view = g_new0(MedialibView, 1);

    view->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(view->container), 4);

    /* 1. Header Toolbar */
    GtkWidget *toolbar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(view->container), toolbar_box, FALSE, FALSE, 2);

    view->search_entry = gtk_search_entry_new();
    gtk_widget_set_tooltip_text(view->search_entry, "Search media library (Title, Artist, Album)");
    gtk_box_pack_start(GTK_BOX(toolbar_box), view->search_entry, TRUE, TRUE, 0);
    g_signal_connect(view->search_entry, "search-changed", G_CALLBACK(on_search_changed), view);

    view->refresh_btn = gtk_button_new_from_icon_name("view-refresh", GTK_ICON_SIZE_BUTTON);
    if (!view->refresh_btn) {
        view->refresh_btn = gtk_button_new_with_label("Refresh");
    }
    gtk_widget_set_tooltip_text(view->refresh_btn, "Refresh library & artwork");
    gtk_box_pack_start(GTK_BOX(toolbar_box), view->refresh_btn, FALSE, FALSE, 0);
    g_signal_connect(view->refresh_btn, "clicked", G_CALLBACK(on_refresh_clicked), view);

    /* 2. Main Tree View */
    view->scrolled_win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view->scrolled_win),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(view->scrolled_win), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(view->container), view->scrolled_win, TRUE, TRUE, 0);

    view->store = gtk_tree_store_new(MEDIALIB_NUM_COLS,
                                     GDK_TYPE_PIXBUF,    /* COL_PIXBUF */
                                     G_TYPE_STRING,      /* COL_TITLE */
                                     G_TYPE_STRING,      /* COL_SUBTITLE */
                                     G_TYPE_POINTER,     /* COL_TRACK */
                                     G_TYPE_INT,         /* COL_NODE_TYPE */
                                     G_TYPE_BOOLEAN,     /* COL_IS_LEAF */
                                     G_TYPE_STRING);     /* COL_PATH_OR_URI */

    view->tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(view->store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view->tree_view), FALSE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view->tree_view), FALSE);
    gtk_container_add(GTK_CONTAINER(view->scrolled_win), view->tree_view);

    /* Main Column: Cover Art + Title */
    GtkTreeViewColumn *col_main = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col_main, "Item");

    GtkCellRenderer *renderer_pixbuf = gtk_cell_renderer_pixbuf_new();
    g_object_set(renderer_pixbuf, "xpad", 4, "ypad", 4, NULL);
    gtk_tree_view_column_pack_start(col_main, renderer_pixbuf, FALSE);
    gtk_tree_view_column_add_attribute(col_main, renderer_pixbuf, "pixbuf", COL_PIXBUF);

    GtkCellRenderer *renderer_text = gtk_cell_renderer_text_new();
    g_object_set(renderer_text, "xpad", 6, "ypad", 4, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_column_pack_start(col_main, renderer_text, TRUE);
    gtk_tree_view_column_add_attribute(col_main, renderer_text, "text", COL_TITLE);

    gtk_tree_view_append_column(GTK_TREE_VIEW(view->tree_view), col_main);

    /* Subtitle Column: Duration / Subtitle info */
    GtkTreeViewColumn *col_sub = gtk_tree_view_column_new();
    GtkCellRenderer *renderer_sub = gtk_cell_renderer_text_new();
    g_object_set(renderer_sub, "xpad", 8, "xalign", 1.0, "foreground", "#888888", NULL);
    gtk_tree_view_column_pack_start(col_sub, renderer_sub, FALSE);
    gtk_tree_view_column_add_attribute(col_sub, renderer_sub, "text", COL_SUBTITLE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view->tree_view), col_sub);

    /* Signals */
    g_signal_connect(view->tree_view, "row-activated", G_CALLBACK(on_row_activated), view);
    g_signal_connect(view->tree_view, "button-press-event", G_CALLBACK(on_tree_view_button_press), view);

    /* 3. Footer Status Bar */
    view->status_label = gtk_label_new("Ready");
    gtk_widget_set_halign(view->status_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(view->container), view->status_label, FALSE, FALSE, 2);

    /* Initial store population */
    medialib_tree_populate_store(view->store, NULL);
    view_update_status(view);

    g_signal_connect(view->container, "destroy", G_CALLBACK(on_view_destroy), view);

    s_active_views = g_slist_append(s_active_views, view);
    return view;
}

/* DeaDBeeF GTKUI Layout Widget callbacks */
static void gtkui_widget_init(ddb_gtkui_widget_t *w) {
    (void)w;
}

static void gtkui_widget_destroy(ddb_gtkui_widget_t *w) {
    if (w) {
        if (w->widget) {
            gtk_widget_destroy(w->widget);
        }
        free(w);
    }
}

static int gtkui_widget_message(ddb_gtkui_widget_t *w, uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2) {
    (void)w;
    (void)ctx;
    (void)p1;
    (void)p2;
    if (id == DB_EV_CONFIGCHANGED) {
        custom_medialib_ui_config_changed();
    } else if (id == DB_EV_PLAYLISTCHANGED) {
        custom_medialib_ui_refresh();
    }
    return 0;
}

ddb_gtkui_widget_t *custom_medialib_widget_create(void) {
    MedialibView *view = create_medialib_view();

    ddb_gtkui_widget_t *w = calloc(1, sizeof(ddb_gtkui_widget_t));
    w->type = "custom_medialib";
    w->widget = view->container;
    w->init = gtkui_widget_init;
    w->destroy = gtkui_widget_destroy;
    w->message = gtkui_widget_message;

    view->gtkui_w = w;

    if (gtkui_plugin && gtkui_plugin->w_override_signals) {
        gtkui_plugin->w_override_signals(view->container, w);
    }

    gtk_widget_show_all(view->container);
    return w;
}

void custom_medialib_ui_config_changed(void) {
    for (GSList *l = s_active_views; l != NULL; l = l->next) {
        MedialibView *v = (MedialibView *)l->data;
        if (v && v->store && v->tree_view) {
            medialib_tree_update_icons(v->store, GTK_TREE_VIEW(v->tree_view));
        }
    }
}

void custom_medialib_ui_refresh(void) {
    for (GSList *l = s_active_views; l != NULL; l = l->next) {
        MedialibView *v = (MedialibView *)l->data;
        if (v && v->store) {
            const gchar *text = gtk_entry_get_text(GTK_ENTRY(v->search_entry));
            medialib_tree_populate_store(v->store, text);
            view_update_status(v);
        }
    }
}

void custom_medialib_ui_cleanup(void) {
    if (s_standalone_window) {
        gtk_widget_destroy(s_standalone_window);
        s_standalone_window = NULL;
    }
}

static gboolean on_standalone_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    (void)widget;
    (void)event;
    (void)user_data;
    s_standalone_window = NULL;
    return FALSE; /* let GTK destroy the window */
}

GtkWidget *custom_medialib_create_window(void) {
    if (s_standalone_window) {
        gtk_window_present(GTK_WINDOW(s_standalone_window));
        return s_standalone_window;
    }

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "DeaDBeeF - Custom Media Library");
    gtk_window_set_default_size(GTK_WINDOW(win), 460, 640);

    if (gtkui_plugin && gtkui_plugin->get_mainwin) {
        GtkWidget *mainwin = gtkui_plugin->get_mainwin();
        if (mainwin && GTK_IS_WINDOW(mainwin)) {
            gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(mainwin));
        }
    }

    MedialibView *view = create_medialib_view();
    gtk_container_add(GTK_CONTAINER(win), view->container);

    g_signal_connect(win, "delete-event", G_CALLBACK(on_standalone_window_delete), NULL);

    s_standalone_window = win;
    gtk_widget_show_all(win);
    return win;
}

void custom_medialib_show_window(void) {
    custom_medialib_create_window();
}
