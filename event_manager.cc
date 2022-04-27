#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>
#include "activity_detector.h"
#include "audio_detector.h"
#include "config_manager.h"
#include "event_manager.h"
#include "logind_manager.h"
#include "process_spawner.h"

namespace Xidlechain {
    // Use a negative value to differentiate it from the other
    // messages we send, which are array indices.
    const int64_t EventManager::idlehint_sentinel = -1;

    EventManager::EventManager(ConfigManager *cfg):
        audio_playing{false},
        activity_commands{cfg->activity_commands},
        activity_detector{NULL},
        cfg{cfg},
        logind_manager{NULL},
        process_spawner{NULL}
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
        if (!activity_detector->init(this)) {
            return false;
        }
        if (!logind_manager->init(this)) {
            return false;
        }
        if (!cfg->ignore_audio && !audio_detector->init(this)) {
            return false;
        }
        if (cfg->idlehint_is_enabled()) {
            // Send the sentinel value so that we know it's not an index
            // for activity_commands
            activity_detector->add_idle_timeout(
                (int64_t)cfg->idlehint_timeout_sec * 1000,
                (gpointer)idlehint_sentinel
            );
        }
        if (cfg->disable_automatic_dpms_activation) {
            process_spawner->exec_cmd_sync("xset dpms 0 0 0");
        }
        if (cfg->disable_screensaver) {
            process_spawner->exec_cmd_sync("xset s off");
        }
        for (int64_t i = 0; i < (int64_t)cfg->activity_commands.size(); i++) {
            const Command &cmd = cfg->activity_commands[i];
            // Send the index of the command so that we know later which
            // command to activate
            activity_detector->add_idle_timeout(cmd.timeout_ms, (gpointer)i);
        }
        return true;
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
        if (cfg->idlehint_is_enabled()) {
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
                for (Command &cmd : activity_commands) {
                    deactivate(cmd);
                }
                set_idle_hint(false);
                break;
            case EVENT_SLEEP:
                if (cfg->wait_before_sleep) {
                    process_spawner->exec_cmd_sync(cfg->sleep_cmd.before_cmd);
                } else {
                    process_spawner->exec_cmd_async(cfg->sleep_cmd.before_cmd);
                }
                break;
            case EVENT_WAKE:
                process_spawner->exec_cmd_async(cfg->sleep_cmd.after_cmd);
                set_idle_hint(false);
                break;
            case EVENT_LOCK:
                process_spawner->exec_cmd_async(cfg->lock_cmd.before_cmd);
                break;
            case EVENT_UNLOCK:
                process_spawner->exec_cmd_async(cfg->lock_cmd.after_cmd);
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
