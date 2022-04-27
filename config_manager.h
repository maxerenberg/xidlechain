#ifndef _CONFIG_MANAGER_H_
#define _CONFIG_MANAGER_H_

#include <string>
#include <vector>

#include <glib.h>

#include "command.h"

using std::char_traits;
using std::string;
using std::vector;

namespace Xidlechain {
    class ConfigManager {
        static const GKeyFileFlags key_file_flags = G_KEY_FILE_KEEP_COMMENTS;
        static constexpr const char *action_prefix = "Action ";
        static constexpr const int action_prefix_len = char_traits<char>::length(action_prefix);

        bool parse_main_section(GKeyFile *key_file, gchar *group);
        bool parse_action_section(GKeyFile *key_file, gchar *group);
    public:
        vector<Command> activity_commands;
        Command sleep_cmd;
        Command lock_cmd;
        bool ignore_audio = false;
        bool wait_before_sleep = true;
        int idlehint_timeout_sec = 0;
        bool disable_automatic_dpms_activation = true;
        bool disable_screensaver = true;

        bool parse_config_file(const string &filename);
        bool idlehint_is_enabled() const;
    };
}

#endif