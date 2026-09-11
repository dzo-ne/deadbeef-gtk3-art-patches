/*
    Automated Verification Test for Custom Media Library Plugin
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <deadbeef/deadbeef.h>

/* Mock config store */
static int mock_conf_show_folder_art = 1;
static int mock_conf_show_track_art = 0;

static int mock_conf_get_int(const char *key, int def) {
    if (strcmp(key, "custom_medialib.show_folder_art") == 0) {
        return mock_conf_show_folder_art;
    }
    if (strcmp(key, "custom_medialib.show_track_art") == 0) {
        return mock_conf_show_track_art;
    }
    return def;
}

static void mock_conf_set_int(const char *key, int val) {
    if (strcmp(key, "custom_medialib.show_folder_art") == 0) {
        mock_conf_show_folder_art = val;
        return;
    }
    if (strcmp(key, "custom_medialib.show_track_art") == 0) {
        mock_conf_show_track_art = val;
        return;
    }
}

static int mock_conf_save(void) {
    return 0;
}

static int mock_sendmessage(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2) {
    (void)id;
    (void)ctx;
    (void)p1;
    (void)p2;
    return 0;
}

static struct DB_plugin_s *mock_plug_get_for_id(const char *id) {
    (void)id;
    return NULL;
}

static void mock_pl_lock(void) {}
static void mock_pl_unlock(void) {}

#include <gtk/gtk.h>

int main(int argc, char **argv) {
    gtk_init_check(&argc, &argv);
    const char *so_path = (argc > 1) ? argv[1] : "./ddb_custom_medialib.so";
    printf("Loading plugin library: %s\n", so_path);

    void *handle = dlopen(so_path, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "FAIL: dlopen error: %s\n", dlerror());
        return 1;
    }
    printf("PASS: Library loaded successfully via dlopen\n");

    /* Test ddb_custom_medialib_load entry point */
    typedef DB_plugin_t *(*load_fn_t)(DB_functions_t *);
    load_fn_t load_fn = (load_fn_t)dlsym(handle, "ddb_custom_medialib_load");
    if (!load_fn) {
        fprintf(stderr, "FAIL: dlsym 'ddb_custom_medialib_load' failed: %s\n", dlerror());
        dlclose(handle);
        return 1;
    }
    printf("PASS: Symbol 'ddb_custom_medialib_load' found\n");

    /* Also verify custom_medialib_load fallback entry point */
    load_fn_t alias_fn = (load_fn_t)dlsym(handle, "custom_medialib_load");
    assert(alias_fn != NULL);
    printf("PASS: Symbol 'custom_medialib_load' alias found\n");

    /* Setup mock API */
    DB_functions_t api;
    memset(&api, 0, sizeof(api));
    api.conf_get_int = mock_conf_get_int;
    api.conf_set_int = mock_conf_set_int;
    api.conf_save = mock_conf_save;
    api.sendmessage = mock_sendmessage;
    api.plug_get_for_id = mock_plug_get_for_id;
    api.pl_lock = mock_pl_lock;
    api.pl_unlock = mock_pl_unlock;

    DB_plugin_t *plugin = load_fn(&api);
    if (!plugin) {
        fprintf(stderr, "FAIL: load_fn returned NULL\n");
        dlclose(handle);
        return 1;
    }
    printf("PASS: Plugin initialized via load_fn\n");

    /* Verify plugin metadata */
    assert(plugin->type == DB_PLUGIN_MISC);
    printf("PASS: Plugin type is DB_PLUGIN_MISC (%d)\n", plugin->type);

    assert(plugin->id != NULL && strcmp(plugin->id, "custom_medialib") == 0);
    printf("PASS: Plugin ID is '%s'\n", plugin->id);

    assert(plugin->name != NULL && strcmp(plugin->name, "Custom Media Library") == 0);
    printf("PASS: Plugin Name is '%s'\n", plugin->name);

    /* Verify configdialog mini-DSL */
    assert(plugin->configdialog != NULL);
    printf("Configdialog mini-DSL:\n%s", plugin->configdialog);

    assert(strstr(plugin->configdialog, "custom_medialib.show_folder_art 1") != NULL);
    printf("PASS: configdialog contains 'custom_medialib.show_folder_art' with default 1\n");

    assert(strstr(plugin->configdialog, "custom_medialib.show_track_art 0") != NULL);
    printf("PASS: configdialog contains 'custom_medialib.show_track_art' with default 0\n");

    /* Verify lifecycle hooks */
    assert(plugin->start != NULL);
    assert(plugin->stop != NULL);
    assert(plugin->connect != NULL);
    assert(plugin->disconnect != NULL);
    assert(plugin->message != NULL);
    printf("PASS: All lifecycle hooks are non-NULL\n");

    /* Test start lifecycle */
    int rc = plugin->start();
    assert(rc == 0);
    printf("PASS: plugin->start() returned 0\n");

    /* Test connect lifecycle */
    rc = plugin->connect();
    assert(rc == 0);
    printf("PASS: plugin->connect() returned 0\n");

    /* Test DB_EV_CONFIGCHANGED event */
    mock_conf_show_folder_art = 0;
    mock_conf_show_track_art = 1;
    rc = plugin->message(DB_EV_CONFIGCHANGED, 0, 0, 0);
    assert(rc == 0);
    printf("PASS: plugin->message(DB_EV_CONFIGCHANGED) succeeded\n");

    /* Test disconnect lifecycle */
    rc = plugin->disconnect();
    assert(rc == 0);
    printf("PASS: plugin->disconnect() returned 0\n");

    /* Test stop lifecycle */
    rc = plugin->stop();
    assert(rc == 0);
    printf("PASS: plugin->stop() returned 0\n");

    dlclose(handle);
    printf("PASS: dlclose completed cleanly\n");
    printf("\nALL AUTOMATED TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}
