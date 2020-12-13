#include <cerrno>
#include "process_spawner.h"

namespace Xidlechain {
    void GProcessSpawner::exec_cmd(char *cmd, bool wait) {
        if (!cmd) return;
        // prepend the command with 'exec' so that |cmd| will receive our
        // SIGTERM signal (instead of the shell)
        gchar *ex_cmd = g_strdup_printf("exec %s", cmd);
        gchar *argv[] = {"sh", "-c", ex_cmd, NULL};
        GSpawnFlags flags = G_SPAWN_SEARCH_PATH;
        GError *err = NULL;
        gboolean success;
        GPid pid;

        if (wait) {
            success = g_spawn_sync(
                NULL, argv, NULL, flags, NULL, NULL, NULL, NULL,
                NULL, &err);
        } else {
            flags = (GSpawnFlags)(flags | G_SPAWN_DO_NOT_REAP_CHILD);
            success = g_spawn_async(
                NULL, argv, NULL, flags, NULL, NULL, &pid, &err);
            if (success) {
                g_child_watch_add(pid, child_watch_cb, this);
                children.insert(pid);
            }
        }
        g_free(ex_cmd);
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

    void GProcessSpawner::child_watch_cb(GPid pid, gint status, gpointer data) {
        GProcessSpawner *this_ = static_cast<GProcessSpawner*>(data);
        this_->children.erase(pid);
        g_spawn_close_pid(pid);
    }

    void GProcessSpawner::kill_children() {
        for (GPid pid : children) {
            if (kill(pid, SIGTERM) != 0) {
                g_warning("kill: %s", strerror(errno));
            }
        }
        children.clear();
    }
}