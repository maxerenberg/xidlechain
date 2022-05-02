#include "config_manager.h"

#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "command.h"
#include "defer.h"

using std::abort;
using std::char_traits;
using std::istringstream;
using std::make_unique;
using std::unique_ptr;
using std::string;
using Xidlechain::Command;

static bool read_bool(GKeyFile *key_file, gchar *group, gchar *key, bool &result, GError *error) {
    result = g_key_file_get_boolean(key_file, group, key, &error);
    if (error != NULL) {
        g_warning("Could not read boolean value of %s: %s", key, error->message);
        return false;
    }
    return true;
}

static bool read_int(GKeyFile *key_file, gchar *group, gchar *key, int &result, GError *error) {
    result = g_key_file_get_integer(key_file, group, key, &error);
    if (error != NULL) {
        g_warning("Could not read integer value of %s: %s", key, error->message);
        return false;
    }
    return true;
}

static bool read_int_from_string(const char *s, int &result) {
    istringstream iss(s);
    iss >> result;
    return !iss.fail();
}

static unique_ptr<Command::Action> read_action(GKeyFile *key_file, gchar *group, gchar *key) {
    GError *error = NULL;
    g_autofree gchar *val = g_key_file_get_value(key_file, group, key, &error);
    g_assert_nonnull(val);
    return Command::Action::factory(val);
}

namespace Xidlechain {
    vector<Command>& ConfigManager::list_for(Command::Trigger trigger) {
        switch (trigger) {
        case Command::TIMEOUT:
            return timeout_commands;
        case Command::SLEEP:
            return sleep_commands;
        case Command::LOCK:
            return lock_commands;
        default:
            g_warning("Unknown command type %d", trigger);
            abort();
        }
    }

    void ConfigManager::add_command(Command &&cmd) {
        list_for(cmd.trigger).emplace_back(std::move(cmd));
    }

    static bool find_command_in_list(vector<Command> &list, const string &name, Command::Trigger trigger, Command **cmd) {
        for (vector<Command>::iterator it = list.begin(); it != list.end(); it++) {
            if (it->name == name) {
                *cmd = &(*it);
                return true;
            }
        }
        return false;
    }

    bool ConfigManager::find_command(const string &name, Command::Trigger trigger, Command **cmd) {
        return find_command_in_list(list_for(trigger), name, trigger, cmd);
    }

    static bool remove_command_from_list(vector<Command> &list, Command &cmd) {
        for (vector<Command>::iterator it = list.begin(); it != list.end(); it++) {
            if (&(*it) == &cmd) {
                list.erase(it);
                return true;
            }
        }
        return false;
    }

    bool ConfigManager::remove_command(Command &cmd) {
        return remove_command_from_list(list_for(cmd.trigger), cmd);
    }

    bool ConfigManager::parse_action_section(GKeyFile *key_file, gchar *group) {
        static constexpr const char * const timeout_prefix = "timeout ";
        static constexpr const int timeout_prefix_len = char_traits<char>::length(timeout_prefix);
        GError *error = NULL;
        gchar **keys = NULL;
        gchar *action_name = group + action_prefix_len;
        Defer defer([&]{
            if (error != NULL) g_error_free(error);
            if (keys != NULL) g_strfreev(keys);
        });

        keys = g_key_file_get_keys(key_file, group, NULL, &error);
        g_assert_nonnull(keys);
        Command cmd;
        cmd.name = string(action_name);
        for (int i = 0; keys[i] != NULL; i++) {
            gchar *key = keys[i];
            if (g_strcmp0(key, "trigger") == 0) {
                gchar *val = g_key_file_get_value(key_file, group, key, &error);
                if (g_str_has_prefix(val, timeout_prefix)) {
                    int timeout_sec;
                    if (!read_int_from_string(val + timeout_prefix_len, timeout_sec)) {
                        g_warning("Timeout value for action %s must be an integer", action_name);
                        return false;
                    }
                    if (timeout_sec <= 0) {
                        g_warning("Timeout for action %s must be positive", action_name);
                        return false;
                    }
                    cmd.trigger = Command::TIMEOUT;
                    cmd.timeout_ms = (int64_t)timeout_sec * 1000;
                } else if (g_strcmp0(val, "sleep") == 0) {
                    cmd.trigger = Command::SLEEP;
                } else if (g_strcmp0(val, "lock") == 0) {
                    cmd.trigger = Command::LOCK;
                } else {
                    g_warning("Unrecognized trigger type '%s' for action '%s'", val, action_name);
                    return false;
                }
            } else if (g_strcmp0(key, "exec") == 0) {
                cmd.activation_action = read_action(key_file, group, key);
            } else if (g_strcmp0(key, "resume_exec") == 0) {
                cmd.deactivation_action = read_action(key_file, group, key);
            } else {
                g_warning("Unrecognized key '%s' in section %s", key, group);
                return false;
            }
        }
        if (cmd.trigger == Command::NONE) {
            g_warning("Action '%s' must specify a trigger type", action_name);
            return false;
        }
        if (!cmd.activation_action && !cmd.deactivation_action) {
            g_warning("Action '%s' must specify either exec or resume_exec", action_name);
            return false;
        }
        add_command(std::move(cmd));
        return true;
    }

    bool ConfigManager::parse_main_section(GKeyFile *key_file, gchar *group) {
        GError *error = NULL;
        gchar **keys = NULL;
        Defer defer([&]{
            if (error != NULL) g_error_free(error);
            if (keys != NULL) g_strfreev(keys);
        });

        keys = g_key_file_get_keys(key_file, group, NULL, &error);
        g_assert_nonnull(keys);
        for (int i = 0; keys[i] != NULL; i++) {
            gchar *key = keys[i];
            if (g_strcmp0(key, "ignore_audio") == 0) {
                if (!read_bool(key_file, group, key, ignore_audio, error)) {
                    return false;
                }
            } else if (g_strcmp0(key, "wait_before_sleep") == 0) {
                if (!read_bool(key_file, group, key, wait_before_sleep, error)) {
                    return false;
                }
            } else if (g_strcmp0(key, "idlehint_timeout") == 0) {
                if (!read_int(key_file, group, key, idlehint_timeout_sec, error)) {
                    return false;
                }
                if (idlehint_timeout_sec < 0) {
                    g_warning("idlehint_timeout must be non-negative");
                    return false;
                }
            } else if (g_strcmp0(key, "disable_automatic_dpms_activation") == 0) {
                if (!read_bool(key_file, group, key, disable_automatic_dpms_activation, error)) {
                    return false;
                }
            } else if (g_strcmp0(key, "disable_screensaver") == 0) {
                if (!read_bool(key_file, group, key, disable_screensaver, error)) {
                    return false;
                }
            } else {
                g_warning("Unrecognized key '%s' in Main section", key);
                return false;
            }
        }
        return true;
    }

    bool ConfigManager::parse_config_file(const string &filename) {
        GKeyFile *key_file = NULL;
        GError *error = NULL;
        gchar **groups = NULL;
        Defer defer([&]{
            if (key_file != NULL) g_key_file_free(key_file);
            if (error != NULL) g_error_free(error);
            if (groups != NULL) g_strfreev(groups);
        });

        key_file = g_key_file_new();
        string config_file_path;
        if (filename == "") {
            // TODO: check XDG_CONFIG_HOME
            char *home = getenv("HOME");
            if (home == NULL) {
                g_warning("HOME environment variable must be set");
                return false;
            }
            config_file_path = home + string("/.config/xidlechain.conf");
        } else {
            config_file_path = filename;
        }
        if (!g_key_file_load_from_file(key_file, config_file_path.c_str(), key_file_flags, &error)) {
            if (error->code == G_FILE_ERROR_NOENT) {
                // TODO: create file if it doesn't exist
            }
            g_warning("Could not read config file %s: %s", config_file_path.c_str(), error->message);
            return false;
        }
        groups = g_key_file_get_groups(key_file, NULL);
        for (int i = 0; groups[i] != NULL; i++) {
            gchar *group = groups[i];
            if (g_strcmp0(groups[i], "Main") == 0) {
                if (!parse_main_section(key_file, group)) {
                    return false;
                }
            } else if (g_str_has_prefix(groups[i], action_prefix)) {
                if (!parse_action_section(key_file, group)) {
                    return false;
                }
            } else {
                g_warning("Unexpected section name '%s'", groups[i]);
                return false;
            }
        }
        return true;
    }

    bool ConfigManager::idlehint_is_enabled() const {
        return idlehint_timeout_sec > 0;
    }
}
