#ifndef _EVENT_MANAGER_H_
#define _EVENT_MANAGER_H_

#include <vector>
#include <glib.h>
#include "activity_manager.h"
#include "event_receiver.h"
#include "logind_manager.h"

using std::vector;

namespace Xidlechain {
    struct Command {
        bool activated;
        char *before_cmd,
             *after_cmd;
        Command();
        Command(char *before_cmd, char *after_cmd);
        ~Command();
    };

    class EventManager: public EventReceiver {
        ActivityManager activity_manager;
        LogindManager logind_manager;
        vector<Command> activity_commands;
        Command sleep_cmd,
                lock_cmd;
        bool idlehint_enabled;
        bool should_wait;
        // Sentinel value to determine which timer went off (i.e. one of
        // the activity timers or the idle hint timer).
        static const int64_t idlehint_sentinel;

        void activate(Command &cmd);
        void deactivate(Command &cmd);
        void exec_cmd(char *cmd);
        void set_idle_hint(bool idle);
    public:
        EventManager();
        bool init();
        void add_timeout_command(char *before_cmd, char *after_cmd,
                                 int64_t timeout_ms);
        void set_sleep_cmd(char *cmd);
        void set_resume_cmd(char *cmd);
        void set_lock_cmd(char *cmd);
        void set_unlock_cmd(char *cmd);
        void set_should_wait(bool wait);
        void enable_idle_hint(int64_t timeout_ms);
        void receive(EventType event, gpointer data) override;
    };
}

#endif