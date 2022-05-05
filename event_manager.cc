#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>
#include "activity_detector.h"
#include "audio_detector.h"
#include "brightness_controller.h"
#include "config_manager.h"
#include "event_manager.h"
#include "logind_manager.h"
#include "process_spawner.h"

namespace Xidlechain {
    EventManager::EventManager(ConfigManager *cfg):
        audio_playing{false},
        activity_detector{NULL},
        cfg{cfg},
        logind_manager{NULL},
        process_spawner{NULL},
        brightness_controller{NULL}
    {}

    bool EventManager::init(
        ActivityDetector *activity_detector,
        LogindManager *logind_manager,
        AudioDetector *audio_detector,
        ProcessSpawner *process_spawner,
        BrightnessController *brightness_controller
    ) {
        g_return_val_if_fail(activity_detector != NULL, FALSE);
        g_return_val_if_fail(logind_manager != NULL, FALSE);
        g_return_val_if_fail(audio_detector != NULL, FALSE);
        g_return_val_if_fail(process_spawner != NULL, FALSE);
        g_return_val_if_fail(brightness_controller != NULL, FALSE);
        this->activity_detector = activity_detector;
        this->logind_manager = logind_manager;
        this->process_spawner = process_spawner;
        this->brightness_controller = brightness_controller;
        if (cfg->disable_automatic_dpms_activation) {
            process_spawner->exec_cmd_sync("xset dpms 0 0 0");
        }
        if (cfg->disable_screensaver) {
            process_spawner->exec_cmd_sync("xset s off");
        }
        add_timeouts();
        return true;
    }

    Command::ActionExecutors EventManager::get_executors() const {
        return {brightness_controller, process_spawner, logind_manager};
    }

    void EventManager::activate(Command &cmd, bool sync) {
        cmd.activate(get_executors(), sync);
    }

    void EventManager::deactivate(Command &cmd, bool sync) {
        cmd.deactivate(get_executors(), sync);
    }

    void EventManager::add_timeouts() {
        for (Command &cmd : cfg->timeout_commands) {
            // Send the pointer of the command so that we know later which
            // command to activate
            activity_detector->add_idle_timeout(cmd.timeout_ms, &cmd);
        }
    }

    void EventManager::clear_timeouts() {
        activity_detector->clear_timeouts();
    }

    void EventManager::handle_config_changed_ignore_audio(const ConfigChangeInfo *cfg_info) {
        bool old_value = g_variant_get_boolean(cfg_info->old_value);
        bool new_value = g_variant_get_boolean(cfg_info->new_value);
        if (old_value == new_value) {
            // no change
            return;
        }
        if (!audio_playing) {
            // nothing to do since timeouts are always enabled
            // if audio is not running
            return;
        }
        if (new_value) {
            // NOT IGNORE AUDIO -> IGNORE AUDIO
            // TIMEOUTS DISABLED -> TIMEOUTS ENABLED
            g_debug("Re-enabling timeouts because we are now ignoring audio");
            add_timeouts();
        } else {
            // IGNORE AUDIO -> NOT IGNORE AUDIO
            // TIMEOUTS ENABLED -> TIMEOUTS DISABLED
            g_debug("Disabling timeouts because we are no longer ignoring audio");
            clear_timeouts();
        }
    }

    void EventManager::receive(EventType event, gpointer data) {
        switch (event) {
        case EVENT_ACTIVITY_TIMEOUT:
            {
                Command *cmd = (Command*)data;
                activate(*cmd);
            }
            break;
        case EVENT_ACTIVITY_RESUME:
            for (Command &cmd : cfg->timeout_commands) {
                deactivate(cmd);
            }
            break;
        case EVENT_SLEEP:
            for (Command &cmd : cfg->sleep_commands) {
                activate(cmd, cfg->wait_before_sleep);
            }
            break;
        case EVENT_WAKE:
            for (Command &cmd : cfg->sleep_commands) {
                deactivate(cmd);
            }
            break;
        case EVENT_LOCK:
            for (Command &cmd : cfg->lock_commands) {
                activate(cmd);
            }
            break;
        case EVENT_UNLOCK:
            for (Command &cmd : cfg->lock_commands) {
                deactivate(cmd);
            }
            break;
        case EVENT_AUDIO_RUNNING:
            if (audio_playing) {  // no change
                break;
            }
            audio_playing = true;
            if (cfg->ignore_audio) {
                g_debug("Audio running; ignoring");
                break;
            }
            g_info("Audio running, disabling timeouts");
            clear_timeouts();
            break;
        case EVENT_AUDIO_STOPPED:
            if (!audio_playing) {  // no change
                break;
            }
            audio_playing = false;
            if (cfg->ignore_audio) {
                g_debug("Audio stopped; ignoring");
                break;
            }
            g_info("Audio stopped, re-enabling timeouts");
            add_timeouts();
            break;
        case EVENT_COMMAND_DELETED:
            {
                Command *cmd = (Command*)data;
                g_debug("Removing command '%s'", cmd->name.c_str());
                if (cmd->trigger == Command::TIMEOUT) {
                    activity_detector->remove_idle_timeout(cmd);
                }
                cfg->remove_command(*cmd);
            }
            break;
        case EVENT_CONFIG_CHANGED:
            {
                const ConfigChangeInfo *cfg_info = (const ConfigChangeInfo*)data;
                if (g_strcmp0(cfg_info->name, "IgnoreAudio") == 0) {
                    handle_config_changed_ignore_audio(cfg_info);
                }
            }
            break;
        default:
            g_warning("Received unknown event type %d", event);
        }
    }
}
