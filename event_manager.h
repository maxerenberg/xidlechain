#ifndef _EVENT_MANAGER_H_
#define _EVENT_MANAGER_H_

#include <unordered_set>
#include <vector>
#include <glib.h>
#include "activity_manager.h"
#include "audio_manager.h"
#include "event_receiver.h"
#include "logind_manager.h"

using std::unordered_set;
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

    class EventManager: public EventReceiver {
        ActivityManager activity_manager;
        LogindManager logind_manager;
        AudioManager audio_manager;
        vector<Command> activity_commands;
        unordered_set<GPid> children;
        Command sleep_cmd,
                lock_cmd;
        bool idlehint_enabled;
        bool wait_before_sleep,
             kill_on_resume,
             audio_playing;
        // Sentinel value to determine which timer went off (i.e. one of
        // the activity timers or the idle hint timer).
        static const int64_t idlehint_sentinel;

        void activate(Command &cmd);
        void deactivate(Command &cmd);
        void exec_cmd(char *cmd, bool wait);
        void set_idle_hint(bool idle);
        void kill_children();
        static void child_watch_cb(GPid pid, gint status, gpointer data);
    public:
        EventManager();
        bool init(bool wait_before_sleep, bool kill_on_resume,
                  bool ignore_audio);
        void add_timeout_command(char *before_cmd, char *after_cmd,
                                 int64_t timeout_ms);
        void set_sleep_cmd(char *cmd);
        void set_resume_cmd(char *cmd);
        void set_lock_cmd(char *cmd);
        void set_unlock_cmd(char *cmd);
        void enable_idle_hint(int64_t timeout_ms);
        void receive(EventType event, gpointer data) override;
    };
}

#endif