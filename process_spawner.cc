#include <cerrno>
#include <glib.h>
#include "process_spawner.h"

namespace Xidlechain {
    void GProcessSpawner::exec_cmd(char *cmd, bool wait) {
        if (!cmd) return;
        gchar *argv[] = {"sh", "-c", cmd, NULL};
        GSpawnFlags flags = G_SPAWN_SEARCH_PATH;
        GError *err = NULL;
        gboolean success;
        GPid pid;

        if (wait) {
            success = g_spawn_sync(
                NULL, argv, NULL, flags, NULL, NULL, NULL, NULL,
                NULL, &err);
        } else {
            success = g_spawn_async(
                NULL, argv, NULL, flags, NULL, NULL, &pid, &err);
        }
        if (!success) {
            g_critical("%s", err->message);
            g_error_free(err);
        }
    }

    void GProcessSpawner::exec_cmd_sync(char *cmd) {
        exec_cmd(cmd, true);
    }

    void GProcessSpawner::exec_cmd_async(char *cmd) {
        exec_cmd(cmd, false);
    }
}
