#ifndef _EVENT_MANAGER_H_
#define _EVENT_MANAGER_H_

#include <memory>
#include <vector>

#include <glib.h>

#include "command.h"
#include "event_receiver.h"

using std::vector;

namespace Xidlechain {
    class ActivityDetector;
    class BrightnessController;
    class ConfigManager;
    class LogindManager;
    class AudioDetector;
    class ProcessSpawner;
    struct ConfigChangeInfo;
    struct CommandChangeInfo;

    class EventManager: public EventReceiver {
        bool audio_playing;
        ActivityDetector *activity_detector;
        ConfigManager *cfg;
        LogindManager *logind_manager;
        ProcessSpawner *process_spawner;
        BrightnessController *brightness_controller;

        Command::ActionExecutors get_executors() const;
        void clear_timeouts();
        void add_timeouts();
        void activate(Command &cmd, bool sync=false);
        void deactivate(Command &cmd, bool sync=false);
        void handle_config_ignore_audio_changed(const ConfigChangeInfo *cfg_info);
        void handle_command_trigger_changed(const CommandChangeInfo *info);
    public:
        EventManager(ConfigManager *cfg);
        // Crude dependency injection.
        bool init(ActivityDetector *activity_detector,
                  LogindManager *logind_manager,
                  AudioDetector *audio_detector,
                  ProcessSpawner *process_spawner,
                  BrightnessController *brightness_controller);
        void receive(EventType event, gpointer data) override;
    };
}

#endif
