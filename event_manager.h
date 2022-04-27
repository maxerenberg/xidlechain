#ifndef _EVENT_MANAGER_H_
#define _EVENT_MANAGER_H_

#include <vector>
#include <glib.h>
#include "command.h"
#include "event_receiver.h"

using std::vector;

namespace Xidlechain {
    class ActivityDetector;
    class ConfigManager;
    class LogindManager;
    class AudioDetector;
    class ProcessSpawner;

    class EventManager: public EventReceiver {
        bool audio_playing;
        vector<Command> &activity_commands;
        ActivityDetector *activity_detector;
        ConfigManager *cfg;
        LogindManager *logind_manager;
        ProcessSpawner *process_spawner;
        // Sentinel value to determine which timer went off (i.e. one of
        // the activity timers or the idle hint timer).
        static const int64_t idlehint_sentinel;

        void activate(Command &cmd);
        void deactivate(Command &cmd);
        void set_idle_hint(bool idle);
    public:
        EventManager(ConfigManager *cfg);
        // Crude dependency injection.
        bool init(ActivityDetector *activity_detector,
                  LogindManager *logind_manager,
                  AudioDetector *audio_detector,
                  ProcessSpawner *process_spawner);
        void receive(EventType event, gpointer data) override;
    };
}

#endif
