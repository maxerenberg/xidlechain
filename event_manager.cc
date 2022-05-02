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
        if (!activity_detector->init(this)) {
            return false;
        }
        if (!logind_manager->init(this)) {
            return false;
        }
        if (!cfg->ignore_audio && !audio_detector->init(this)) {
            return false;
        }
        if (!brightness_controller->init(logind_manager)) {
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
        for (Command &cmd : cfg->timeout_commands) {
            // Send the pointer of the command so that we know later which
            // command to activate
            activity_detector->add_idle_timeout(cmd.timeout_ms, (gpointer)&cmd);
        }
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

    void EventManager::set_idle_hint(bool idle) {
        if (cfg->idlehint_is_enabled()) {
            logind_manager->set_idle_hint(idle);
        }
    }

    void EventManager::receive(EventType event, gpointer data) {
        switch (event) {
        case EVENT_ACTIVITY_TIMEOUT:
            if ((int64_t)data == idlehint_sentinel) {
                set_idle_hint(true);
            } else {
                Command *cmd = (Command*)data;
                activate(*cmd);
            }
            break;
        case EVENT_ACTIVITY_RESUME:
            for (Command &cmd : cfg->timeout_commands) {
                deactivate(cmd);
            }
            set_idle_hint(false);
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
            set_idle_hint(false);
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
            for (Command &cmd : cfg->timeout_commands) {
                activity_detector->add_idle_timeout(
                    cmd.timeout_ms, (gpointer)&cmd
                );
            }
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
        }
    }
}
