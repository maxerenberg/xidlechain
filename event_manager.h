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

    class EventManager: public EventReceiver {
        bool audio_playing;
        ActivityDetector *activity_detector;
        ConfigManager *cfg;
        LogindManager *logind_manager;
        ProcessSpawner *process_spawner;
        BrightnessController *brightness_controller;
        // Sentinel value to indicate that we need to activate the idle hint.
        static const int64_t idlehint_sentinel = -1;

        Command::ActionExecutors get_executors() const;
        void activate(Command &cmd, bool sync=false);
        void deactivate(Command &cmd, bool sync=false);
        void set_idle_hint(bool idle);
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
