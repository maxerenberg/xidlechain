#include <cstdlib>
#include <cstdint>
#include <sys/wait.h>
#include <unistd.h>

#include "activity_detector.h"
#include "audio_detector.h"
#include "brightness_controller.h"
#include "config_manager.h"
#include "event_manager.h"
#include "logind_manager.h"
#include "map.h"
#include "process_spawner.h"

using std::uintptr_t;

namespace Xidlechain {
    EventManager::EventManager(ConfigManager *cfg):
        audio_playing{false},
        paused{false},
        timeouts_are_enabled{false},
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
        enable_all_timeouts();
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

    void EventManager::enable_timeout_for_new_command(Command &cmd) {
        g_assert(cmd.trigger == Command::TIMEOUT);
        if (timeouts_should_be_disabled()) {
            // cmd will get added later once timeouts are re-enabled
            g_debug("Not adding '%s' because timeouts are disabled", cmd.name.c_str());
            return;
        }
        activity_detector->add_idle_timeout(cmd.timeout_ms, (gpointer)(long)cmd.id);
    }

    void EventManager::disable_timeout_for_deleted_command(Command &cmd) {
        g_assert(cmd.trigger == Command::TIMEOUT);
        if (!timeouts_are_enabled) {
            g_debug("Not removing '%s' because timeouts are already disabled", cmd.name.c_str());
            return;
        }
        activity_detector->remove_idle_timeout((gpointer)(long)cmd.id);
    }

    void EventManager::enable_all_timeouts() {
        if (timeouts_are_enabled) {
            g_debug("Timeouts are already enabled");
            return;
        }
        if (timeouts_should_be_disabled()) {
            g_debug("Timeouts remain disabled");
            return;
        }
        for (shared_ptr<Command> cmd : cfg->get_timeout_commands()) {
            activity_detector->add_idle_timeout(cmd->timeout_ms, (gpointer)(long)cmd->id);
        }
        timeouts_are_enabled = true;
    }

    void EventManager::disable_all_timeouts() {
        if (!timeouts_are_enabled) {
            g_debug("Timeouts are already disabled");
            return;
        }
        activity_detector->clear_timeouts();
        timeouts_are_enabled = false;
    }

    void EventManager::handle_config_ignore_audio_changed(const ConfigChangeInfo *info) {
        bool old_value = g_variant_get_boolean(info->old_value);
        bool new_value = cfg->ignore_audio;
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
            enable_all_timeouts();
        } else {
            // IGNORE AUDIO -> NOT IGNORE AUDIO
            // TIMEOUTS ENABLED -> TIMEOUTS DISABLED
            g_debug("Disabling timeouts because we are no longer ignoring audio");
            disable_all_timeouts();
        }
    }

    void EventManager::handle_command_trigger_changed(const CommandChangeInfo *info) {
        shared_ptr<Command> cmd = info->cmd;
        Command::Trigger old_trigger = (Command::Trigger) g_variant_get_int32(info->old_value);
        Command::Trigger new_trigger = cmd->trigger;

        if (old_trigger == Command::Trigger::TIMEOUT) {
            g_debug("Removing old timeout for '%s'", cmd->name.c_str());
            // Since the trigger changed, the old version of this
            // command is effectively deleted.
            disable_timeout_for_deleted_command(*cmd);
        }
        if (new_trigger == Command::Trigger::TIMEOUT) {
            g_debug("Adding new timeout for '%s'", cmd->name.c_str());
            // Since the trigger changed, the new version of this
            // command is effectively a new command.
            enable_timeout_for_new_command(*cmd);
        }
    }

    void EventManager::handle_activity_resumed() {
        for (shared_ptr<Command> cmd : cfg->get_timeout_commands()) {
            deactivate(*cmd);
        }
    }

    /* Possible transitions:
     * disabled and should be enabled -> enabled and should be enabled
     * enabled and should be enabled -> enabled and should be disabled
     * enabled and should be disabled -> disabled and should be disabled
     * disabled and should be disabled -> disabled and should be enabled
     */
    bool EventManager::timeouts_should_be_disabled() const {
        return (audio_playing && !cfg->ignore_audio) || paused;
    }

    void EventManager::receive(EventType event, gpointer data) {
        switch (event) {
        case EVENT_ACTIVITY_TIMEOUT:
            {
                shared_ptr<Command> cmd = cfg->lookup_command((uintptr_t)data);
                activate(*cmd);
            }
            break;
        case EVENT_ACTIVITY_RESUME:
            handle_activity_resumed();
            break;
        case EVENT_SLEEP:
            for (shared_ptr<Command> cmd : cfg->get_sleep_commands()) {
                activate(*cmd, cfg->wait_before_sleep);
            }
            break;
        case EVENT_WAKE:
            for (shared_ptr<Command> cmd : cfg->get_sleep_commands()) {
                deactivate(*cmd);
            }
            if (cfg->wake_resumes_activity) {
                handle_activity_resumed();
            }
            break;
        case EVENT_LOCK:
            for (shared_ptr<Command> cmd : cfg->get_lock_commands()) {
                activate(*cmd);
            }
            break;
        case EVENT_UNLOCK:
            for (shared_ptr<Command> cmd : cfg->get_lock_commands()) {
                deactivate(*cmd);
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
            g_debug("Audio running, disabling timeouts");
            disable_all_timeouts();
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
            g_debug("Audio stopped, re-enabling timeouts");
            enable_all_timeouts();
            break;
        case EVENT_CONFIG_CHANGED:
            {
                const ConfigChangeInfo *info = (const ConfigChangeInfo*)data;
                if (g_strcmp0(info->name, "IgnoreAudio") == 0) {
                    handle_config_ignore_audio_changed(info);
                }
            }
            break;
        case EVENT_COMMAND_CHANGED:
            {
                const CommandChangeInfo *info = (const CommandChangeInfo*)data;
                if (g_strcmp0(info->name, "Trigger") == 0) {
                    handle_command_trigger_changed(info);
                }
            }
            break;
        case EVENT_COMMAND_ADDED:
            {
                int id = (uintptr_t)data;
                shared_ptr<Command> cmd = cfg->lookup_command(id);
                if (cmd->trigger == Command::TIMEOUT) {
                    enable_timeout_for_new_command(*cmd);
                }
            }
            break;
        case EVENT_COMMAND_REMOVED:
            {
                const RemovedCommandInfo *info = (const RemovedCommandInfo*)data;
                if (info->cmd->trigger == Command::TIMEOUT) {
                    disable_timeout_for_deleted_command(*info->cmd);
                }
            }
            break;
        case EVENT_PAUSED:
            paused = true;
            disable_all_timeouts();
            break;
        case EVENT_UNPAUSED:
            paused = false;
            enable_all_timeouts();
            break;
        default:
            g_warning("Received unknown event type %d", event);
        }
    }
}
