#ifndef _CONFIG_MANAGER_H_
#define _CONFIG_MANAGER_H_

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

#include <glib.h>

#include "command.h"

using std::char_traits;
using std::string;
using std::shared_ptr;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

namespace Xidlechain {
    class ConfigManager {
        static constexpr const char *action_prefix = "Action ";
        static constexpr const int action_prefix_len = char_traits<char>::length(action_prefix);

        unordered_map<int, shared_ptr<Command>> id_to_command;
        int command_id_counter = 0;

        bool parse_main_section(GKeyFile *key_file, gchar *group);
        bool parse_action_section(GKeyFile *key_file, gchar *group);
        vector<shared_ptr<Command>>& list_for(Command::Trigger trigger);
    public:
        // We need to store pointers in the vectors because we actually
        // store the pointers in other places too, and we can't
        // keep a pointer to a vector entry's slot (will disappear when
        // the vector reallocates memory)
        vector<shared_ptr<Command>> timeout_commands;
        vector<shared_ptr<Command>> sleep_commands;
        vector<shared_ptr<Command>> lock_commands;

        bool ignore_audio = false;
        bool set_ignore_audio(bool value);

        bool wait_before_sleep = true;
        bool set_wait_before_sleep(bool value);

        bool disable_automatic_dpms_activation = true;
        bool set_disable_automatic_dpms_activation(bool value);

        bool disable_screensaver = true;
        bool set_disable_screensaver(bool value);

        bool enable_dbus = true;
        bool set_enable_dbus(bool value);

        bool parse_config_file(const string &filename);
        shared_ptr<Command> lookup_command(int cmd_id);
        bool set_command_name(Command &cmd, const char *val);
        bool set_command_trigger(Command &cmd, const char *val);
        bool set_command_activation_action(Command &cmd, const char *val);
        bool set_command_deactivation_action(Command &cmd, const char *val);
        int add_command(unique_ptr<Command> &&cmd);
        int add_command(shared_ptr<Command> cmd);
        bool remove_command(int cmd_id);
    };

    struct ConfigChangeInfo {
        const gchar *name;
        GVariant *old_value;
    };
    struct CommandChangeInfo: public ConfigChangeInfo {
        shared_ptr<Command> cmd;
    };
    struct RemovedCommandInfo {
        int id;
        Command::Trigger trigger;
    };
}

#endif
