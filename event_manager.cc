#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>
#include "activity_detector.h"
#include "audio_detector.h"
#include "event_manager.h"
#include "logind_manager.h"
#include "process_spawner.h"

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

    EventManager::EventManager(bool wait_before_sleep, bool kill_on_resume,
                               bool ignore_audio):
        wait_before_sleep(wait_before_sleep),
        kill_on_resume(kill_on_resume),
        ignore_audio(ignore_audio),
        idlehint_enabled(false),
        audio_playing(false),
        activity_detector(NULL),
        logind_manager(NULL),
        process_spawner(NULL)
    {}

    bool EventManager::init(ActivityDetector *activity_detector,
                            LogindManager *logind_manager,
                            AudioDetector *audio_detector,
                            ProcessSpawner *process_spawner)
    {
        g_return_val_if_fail(activity_detector != NULL, FALSE);
        g_return_val_if_fail(logind_manager != NULL, FALSE);
        g_return_val_if_fail(audio_detector != NULL, FALSE);
        g_return_val_if_fail(process_spawner != NULL, FALSE);
        this->activity_detector = activity_detector;
        this->logind_manager = logind_manager;
        this->process_spawner = process_spawner;
        return activity_detector->init(this)
            && logind_manager->init(this)
            && (ignore_audio ? true : audio_detector->init(this));
    }

    void EventManager::add_timeout_command(char *before_cmd, char *after_cmd,
                                           int64_t timeout_ms)
    {
        g_return_if_fail(timeout_ms > 1);
        int64_t i = activity_commands.size();
        activity_commands.emplace_back(
            Command{before_cmd, after_cmd, timeout_ms});
        // Send the index of the command so that we know later which
        // command to activate
        activity_detector->add_idle_timeout(timeout_ms, (gpointer)i);
    }

    void EventManager::set_sleep_cmd(char *cmd) {
        sleep_cmd.before_cmd = cmd;
    }

    void EventManager::set_wake_cmd(char *cmd) {
        sleep_cmd.after_cmd = cmd;
    }

    void EventManager::set_lock_cmd(char *cmd) {
        lock_cmd.before_cmd = cmd;
    }

    void EventManager::set_unlock_cmd(char *cmd) {
        lock_cmd.after_cmd = cmd;
    }

    void EventManager::enable_idle_hint(int64_t timeout_ms) {
        if (idlehint_enabled) {
            g_warning("Idle hint timeout can only be set once");
            return;
        }
        g_return_if_fail(timeout_ms > 1);
        idlehint_enabled = true;
        // Send the sentinel value so that we know it's not an index
        // for activity_commands
        activity_detector->add_idle_timeout(
            timeout_ms, (gpointer)idlehint_sentinel);
        // Set the idle hint to false when we first start
        set_idle_hint(false);
    }

    void EventManager::activate(Command &cmd) {
        if (cmd.activated) return;
        process_spawner->exec_cmd_async(cmd.before_cmd);
        cmd.activated = true;
    }

    void EventManager::deactivate(Command &cmd) {
        if (!cmd.activated) return;
        process_spawner->exec_cmd_async(cmd.after_cmd);
        cmd.activated = false;
    }

    void EventManager::set_idle_hint(bool idle) {
        if (idlehint_enabled) {
            logind_manager->set_idle_hint(idle);
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
                if (kill_on_resume) {
                    process_spawner->kill_children();
                }
                for (Command &cmd : activity_commands) {
                    deactivate(cmd);
                }
                set_idle_hint(false);
                break;
            case EVENT_SLEEP:
                if (wait_before_sleep) {
                    process_spawner->exec_cmd_sync(sleep_cmd.before_cmd);
                } else {
                    process_spawner->exec_cmd_async(sleep_cmd.before_cmd);
                }
                break;
            case EVENT_WAKE:
                process_spawner->exec_cmd_async(sleep_cmd.after_cmd);
                set_idle_hint(false);
                break;
            case EVENT_LOCK:
                process_spawner->exec_cmd_async(lock_cmd.before_cmd);
                break;
            case EVENT_UNLOCK:
                process_spawner->exec_cmd_async(lock_cmd.after_cmd);
                set_idle_hint(false);
                break;
            case EVENT_AUDIO_RUNNING:
                if (audio_playing) {  // no change
                    break;
                }
                g_info("Audio running, disabling timeouts");
                audio_playing = true;
                activity_detector->clear_timeouts();
                break;
            case EVENT_AUDIO_STOPPED:
                if (!audio_playing) {  // no change
                    break;
                }
                g_info("Audio stopped, re-enabling timeouts");
                audio_playing = false;
                // re-register all of our timeouts
                for (i = 0; i < (int64_t)activity_commands.size(); i++) {
                    activity_detector->add_idle_timeout(
                        activity_commands[i].timeout_ms,
                        (gpointer)i);
                }
                break;
        }
    }
}
