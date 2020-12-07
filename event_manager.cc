#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>
#include "event_manager.h"

namespace Xidlechain {
    Command::Command():
         activated(false), before_cmd(NULL), after_cmd(NULL),
         timeout_ms(0)
    {}

    Command::Command(char *before_cmd, char *after_cmd, int64_t timeout_ms):
         activated(false), before_cmd(before_cmd), after_cmd(after_cmd),
         timeout_ms(timeout_ms)
    {}

    Command::~Command() {}

    // Use a negative value to differentiate it from the other
    // messages we send, which are array indices.
    const int64_t EventManager::idlehint_sentinel = -1;

    EventManager::EventManager():
        idlehint_enabled(false), wait_before_sleep(true),
        kill_on_resume(true), audio_playing(false)
    {}

    bool EventManager::init(bool wait_before_sleep, bool kill_on_resume,
                            bool ignore_audio)
    {
        this->wait_before_sleep = wait_before_sleep;
        this->kill_on_resume = kill_on_resume;
        return activity_manager.init(this)
            && logind_manager.init(this)
            && (ignore_audio ? true : audio_manager.init(this));
    }

    void EventManager::add_timeout_command(char *before_cmd, char *after_cmd,
                                           int64_t timeout_ms)
    {
        g_return_if_fail(timeout_ms > 1000);
        int64_t i = activity_commands.size();
        activity_commands.emplace_back(
            Command{before_cmd, after_cmd, timeout_ms});
        // Send the index of the command so that we know later which
        // command to activate
        activity_manager.add_idle_timeout(timeout_ms, (gpointer)i);
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

    void EventManager::exec_cmd(char *cmd, bool wait=false) {
        if (!cmd) return;
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

    void EventManager::child_watch_cb(GPid pid, gint status, gpointer data) {
        EventManager *_this = static_cast<EventManager*>(data);
        _this->children.erase(pid);
        g_spawn_close_pid(pid);
    }

    void EventManager::kill_children() {
        if (kill_on_resume) {
            for (GPid pid : children) {
                if (kill(pid, SIGTERM) != 0) {
                    g_warning("kill: %s", strerror(errno));
                }
            }
        }
        children.clear();
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
                kill_children();
                for (Command &cmd : activity_commands) {
                    deactivate(cmd);
                }
                set_idle_hint(false);
                break;
            case EVENT_SLEEP:
                exec_cmd(sleep_cmd.before_cmd, wait_before_sleep);
                break;
            case EVENT_WAKE:
                exec_cmd(sleep_cmd.after_cmd);
                set_idle_hint(false);
                break;
            case EVENT_LOCK:
                exec_cmd(lock_cmd.before_cmd);
                break;
            case EVENT_UNLOCK:
                exec_cmd(lock_cmd.after_cmd);
                set_idle_hint(false);
                break;
            case EVENT_AUDIO_RUNNING:
                if (audio_playing) {  // no change
                    break;
                }
                g_info("Audio running, disabling timeouts");
                audio_playing = true;
                activity_manager.clear_timeouts();
                break;
            case EVENT_AUDIO_STOPPED:
                if (!audio_playing) {  // no change
                    break;
                }
                g_info("Audio stopped, re-enabling timeouts");
                audio_playing = false;
                // re-register all of our timeouts
                for (i = 0; i < (int64_t)activity_commands.size(); i++) {
                    activity_manager.add_idle_timeout(
                        activity_commands[i].timeout_ms,
                        (gpointer)i);
                }
                break;
        }
    }
}
