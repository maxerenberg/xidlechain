#ifndef _CONFIG_MANAGER_H_
#define _CONFIG_MANAGER_H_

#include <string>
#include <memory>
#include <vector>

#include <glib.h>

#include "command.h"

using std::char_traits;
using std::string;
using std::unique_ptr;
using std::vector;

namespace Xidlechain {
    class ConfigManager {
        static const GKeyFileFlags key_file_flags = G_KEY_FILE_KEEP_COMMENTS;
        static constexpr const char *action_prefix = "Action ";
        static constexpr const int action_prefix_len = char_traits<char>::length(action_prefix);

        bool parse_main_section(GKeyFile *key_file, gchar *group);
        bool parse_action_section(GKeyFile *key_file, gchar *group);
        vector<Command>& list_for(Command::Trigger trigger);
    public:
        vector<Command> timeout_commands;
        vector<Command> sleep_commands;
        vector<Command> lock_commands;

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
        void add_command(Command &&cmd);
        bool find_command(const string &name, Command::Trigger trigger, Command **cmd);
        bool remove_command(Command &cmd);
    };

    struct ConfigChangeInfo {
        const gchar *name;
        GVariant *old_value;
        GVariant *new_value;
    };
}

#endif
