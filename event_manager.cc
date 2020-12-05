#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>
#include "event_manager.h"

using namespace std;

namespace Xidlechain {
    Command::Command():
         activated(false), before_cmd(NULL), after_cmd(NULL)
    {}

    Command::Command(char *before_cmd, char *after_cmd):
         activated(false), before_cmd(before_cmd), after_cmd(after_cmd)
    {}

    Command::~Command() {}

    // Use a negative value to differentiate it from the other
    // messages we send, which are array indices.
    const int64_t EventManager::idlehint_sentinel = -1;

    EventManager::EventManager():
        idlehint_enabled(false), should_wait(false)
    {}

    bool EventManager::init() {
        return activity_manager.init(this)
            && logind_manager.init(this);
    }

    void EventManager::add_timeout_command(char *before_cmd, char *after_cmd,
                                           int64_t timeout_ms)
    {
        int64_t i = activity_commands.size();
        activity_commands.emplace_back(Command{before_cmd, after_cmd});
        // Send the index of the command so that we know later which
        // command to activate
        activity_manager.add_idle_timeout(
            timeout_ms, (gpointer)i);
    }

    void EventManager::set_sleep_cmd(char *cmd) {
        sleep_cmd.before_cmd = cmd;
    }

    void EventManager::set_resume_cmd(char *cmd) {
        sleep_cmd.after_cmd = cmd;
    }

    void EventManager::set_lock_cmd(char *cmd) {
        lock_cmd.before_cmd = cmd;
    }

    void EventManager::set_unlock_cmd(char *cmd) {
        lock_cmd.after_cmd = cmd;
    }

    void EventManager::set_should_wait(bool wait) {
        should_wait = wait;
    }

    void EventManager::exec_cmd(char *cmd) {
        if (!cmd) return;
        gchar *argv[] = {"sh", "-c", cmd, NULL};
        GSpawnFlags flags = G_SPAWN_SEARCH_PATH;
        GError *err = NULL;
        gboolean success;

        if (should_wait) {
            success = g_spawn_sync(
                NULL, argv, NULL, flags, NULL, NULL, NULL, NULL,
                NULL, &err);
        } else {
            success = g_spawn_async(
                NULL, argv, NULL, flags, NULL, NULL, NULL, &err);
        }
        if (!success) {
            g_critical("%s", err->message);
            g_error_free(err);
        }
    }

    void EventManager::enable_idle_hint(int64_t timeout_ms) {
        if (idlehint_enabled) {
            g_warning("Idle hint timeout can only be set once");
            return;
        }
        idlehint_enabled = true;
        // Send the sentinel value so that we know it's not an index
        // for activity_commands
        activity_manager.add_idle_timeout(
            timeout_ms, (gpointer)idlehint_sentinel);
        // Set the idle hint to false when we first start
        set_idle_hint(false);
    }

    void EventManager::activate(Command &cmd) {
        if (cmd.activated) return;
        exec_cmd(cmd.before_cmd);
        cmd.activated = true;
    }

    void EventManager::deactivate(Command &cmd) {
        if (!cmd.activated) return;
        exec_cmd(cmd.after_cmd);
        cmd.activated = false;
    }

    void EventManager::set_idle_hint(bool idle) {
        if (idlehint_enabled) {
            logind_manager.set_idle_hint(idle);
        }
    }

    void EventManager::receive(EventType event, gpointer data) {
        int64_t i = (int64_t)data;
        switch (event) {
            case EVENT_ACTIVITY_TIMEOUT:
                if (i == idlehint_sentinel) {
                    set_idle_hint(true);
                } else {
                    activate(activity_commands[i]);
                }
                break;
            case EVENT_ACTIVITY_RESUME:
                for (Command &cmd : activity_commands) {
                    deactivate(cmd);
                }
                set_idle_hint(false);
                break;
            case EVENT_SLEEP:
                activate(sleep_cmd);
                break;
            case EVENT_WAKE:
                deactivate(sleep_cmd);
                set_idle_hint(false);
                break;
            case EVENT_LOCK:
                activate(lock_cmd);
                break;
            case EVENT_UNLOCK:
                deactivate(lock_cmd);
                set_idle_hint(false);
                break;
        }
    }
}