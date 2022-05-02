#include "process_spawner.h"

#include <cerrno>
#include <string>

#include <glib.h>

using std::string;

namespace Xidlechain {
    void GProcessSpawner::exec_cmd(const string &cmd, bool wait) {
        if (cmd.empty()) return;
        g_debug("Executing command '%s'", cmd.c_str());
        const gchar *argv[] = {"sh", "-c", cmd.c_str(), NULL};
        GSpawnFlags flags = G_SPAWN_SEARCH_PATH;
        GError *err = NULL;
        gboolean success;

        /*
        Internally, GLib casts argv to const gchar * const *,
        so it should be OK to cast argv to gchar **.
        See https://gitlab.gnome.org/GNOME/glib/-/blob/main/glib/gspawn.c
        */

        if (wait) {
            success = g_spawn_sync(
                NULL, (gchar**)argv, NULL, flags, NULL, NULL, NULL, NULL,
                NULL, &err);
        } else {
            success = g_spawn_async(
                NULL, (gchar**)argv, NULL, flags, NULL, NULL, NULL, &err);
        }
        if (!success) {
            g_critical("%s", err->message);
            g_error_free(err);
        }
    }

    void GProcessSpawner::exec_cmd_sync(const string &cmd) {
        exec_cmd(cmd, true);
    }

    void GProcessSpawner::exec_cmd_async(const string &cmd) {
        exec_cmd(cmd, false);
    }
}
