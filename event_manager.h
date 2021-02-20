#ifndef _EVENT_MANAGER_H_
#define _EVENT_MANAGER_H_

#include <vector>
#include <glib.h>
#include "event_receiver.h"

using std::vector;

namespace Xidlechain {
    struct Command {
        bool activated;
        char *before_cmd,
             *after_cmd;
        int64_t timeout_ms;
        Command();
        Command(char *before_cmd, char *after_cmd, int64_t timeout_ms);
        ~Command();
    };

    class ActivityDetector;
    class LogindManager;
    class AudioDetector;
    class ProcessSpawner;

    class EventManager: public EventReceiver {
        vector<Command> activity_commands;
        Command sleep_cmd,
                lock_cmd;
        bool wait_before_sleep,
             ignore_audio,
             idlehint_enabled,
             audio_playing;
        ActivityDetector *activity_detector;
        LogindManager *logind_manager;
        ProcessSpawner *process_spawner;
        // Sentinel value to determine which timer went off (i.e. one of
        // the activity timers or the idle hint timer).
        static const int64_t idlehint_sentinel;

        void activate(Command &cmd);
        void deactivate(Command &cmd);
        void set_idle_hint(bool idle);
    public:
        EventManager(bool wait_before_sleep, bool ignore_audio);
        // Crude dependency injection.
        bool init(ActivityDetector *activity_detector,
                  LogindManager *logind_manager,
                  AudioDetector *audio_detector,
                  ProcessSpawner *process_spawner);
        void add_timeout_command(char *before_cmd, char *after_cmd,
                                 int64_t timeout_ms);
        void set_sleep_cmd(char *cmd);
        void set_wake_cmd(char *cmd);
        void set_lock_cmd(char *cmd);
        void set_unlock_cmd(char *cmd);
        // Calling this method will set the IdleHint to true after
        // |timeout_ms| of inactivity. Once activity has resumed, the
        // IdleHint is set to false.
        void enable_idle_hint(int64_t timeout_ms);
        void receive(EventType event, gpointer data) override;
    };
}

#endif
