#include "config_manager.h"

#include <cstdlib>
#include <memory>
#include <fstream>
#include <string>
#include <unordered_set>
#include <utility>

#include "command.h"
#include "filter.h"
#include "map.h"

using std::abort;
using std::char_traits;
using std::ofstream;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::unordered_set;
using Xidlechain::Command;

static string get_xdg_config_home() {
    char *xdg_config_home = getenv("XDG_CONFIG_HOME");
    if (xdg_config_home != NULL) {
        return string(xdg_config_home);
    }
    char *home = getenv("HOME");
    if (home == NULL) {
        g_warning("HOME environment variable is not set");
        return string{};
    }
    return home + string("/.config");
}

static string get_default_config_file_path() {
    string &&xdg_config_home = get_xdg_config_home();
    if (xdg_config_home.empty()) {
        return string{};
    }
    return xdg_config_home + "/xidlechain.conf";
}

static const char* bool_to_str(bool val) {
    return val ? "true" : "false";
}

static bool read_bool(GKeyFile *key_file, gchar *group, gchar *key, bool &result) {
    g_autoptr(GError) error = NULL;
    result = g_key_file_get_boolean(key_file, group, key, &error);
    if (error != NULL) {
        g_warning("Could not read boolean value of %s: %s", key, error->message);
        return false;
    }
    return true;
}

// Returns null iff val is invalid or empty.
static unique_ptr<Command::Action> get_action(const char *val) {
    g_autoptr(GError) error = NULL;
    unique_ptr<Command::Action> action = Command::Action::factory(val, &error);
    if (error) {
        g_warning("Invalid action value '%s': %s", val, error->message);
    }
    return action;
}

static unique_ptr<Command::Action> read_action(GKeyFile *key_file, gchar *group, gchar *key) {
    g_autofree gchar *val = g_key_file_get_value(key_file, group, key, NULL);
    g_assert_nonnull(val);
    return get_action(val);
}

namespace Xidlechain {
    CommandMapValues ConfigManager::get_all_commands() {
        // For some reason, when I defined this inline in the Map
        // constructor, it didn't work. I'm still not sure why.
        std::function<
            shared_ptr<Command>(std::pair<const int, shared_ptr<Command>>)
        > f = [](std::pair<const int, shared_ptr<Command>> p) {
            return p.second;
        };
        return Map(
            id_to_command.begin(),
            id_to_command.end(),
            std::move(f)
        );
    }

    template<Command::Trigger trigger>
    FilteredCommands ConfigManager::get_commands() {
        return BoxFilter(
            get_all_commands(),
            [](shared_ptr<Command> cmd){
                return cmd->trigger == trigger;
            }
        );
    }

    FilteredCommands ConfigManager::get_timeout_commands() {
        return get_commands<Command::TIMEOUT>();
    }

    FilteredCommands ConfigManager::get_sleep_commands() {
        return get_commands<Command::SLEEP>();
    }

    FilteredCommands ConfigManager::get_lock_commands() {
        return get_commands<Command::LOCK>();
    }

    int ConfigManager::add_command(shared_ptr<Command> cmd) {
        g_assert(cmd->id == 0);
        int id = cmd->id = ++command_id_counter;
        id_to_command.emplace(id, std::move(cmd));
        return id;
    }

    int ConfigManager::add_command(unique_ptr<Command> &&cmd) {
        return add_command(shared_ptr<Command>(std::move(cmd)));
    }

    bool ConfigManager::remove_command(int cmd_id) {
        return id_to_command.erase(cmd_id) == 1;
    }

    shared_ptr<Command> ConfigManager::lookup_command(int cmd_id) {
        unordered_map<int, shared_ptr<Command>>::iterator it = id_to_command.find(cmd_id);
        if (it == id_to_command.end()) {
            return shared_ptr<Command>(nullptr);
        }
        return it->second;
    }

    bool ConfigManager::set_command_name(Command &cmd, const char *val) {
        cmd.name = string(val);
        return true;
    }

    bool ConfigManager::set_command_trigger(Command &cmd, const char *val) {
        g_autoptr(GError) error = NULL;
        if (!cmd.set_trigger_from_str(val, &error)) {
            g_warning("Could not set trigger for action %d: %s", cmd.id, error->message);
            return false;
        }
        return true;
    }

    bool ConfigManager::set_command_activation_action(Command &cmd, const char *val) {
        unique_ptr<Command::Action> action = get_action(val);
        if (!action && *val) return false;
        cmd.activation_action = std::move(action);
        return true;
    }

    bool ConfigManager::set_command_deactivation_action(Command &cmd, const char *val) {
        unique_ptr<Command::Action> action = get_action(val);
        if (!action && *val) return false;
        cmd.deactivation_action = std::move(action);
        return true;
    }

    bool ConfigManager::parse_action_section(GKeyFile *key_file, gchar *group) {
        g_autoptr(GError) error = NULL;
        g_auto(GStrv) keys = NULL;
        gchar *action_name = group + action_prefix_len;

        keys = g_key_file_get_keys(key_file, group, NULL, NULL);
        g_assert_nonnull(keys);
        unique_ptr<Command> cmd = make_unique<Command>();
        set_command_name(*cmd, action_name);
        for (int i = 0; keys[i] != NULL; i++) {
            gchar *key = keys[i];
            if (g_strcmp0(key, "trigger") == 0) {
                gchar *val = g_key_file_get_value(key_file, group, key, NULL);
                if (!set_command_trigger(*cmd, val)) {
                    return false;
                }
            } else if (g_strcmp0(key, "exec") == 0) {
                cmd->activation_action = read_action(key_file, group, key);
            } else if (g_strcmp0(key, "resume_exec") == 0) {
                cmd->deactivation_action = read_action(key_file, group, key);
            } else {
                g_warning("Unrecognized key '%s' in section %s", key, group);
                return false;
            }
        }
        if (!cmd->is_valid(&error)) {
            g_warning(error->message);
            return false;
        }
        add_command(std::move(cmd));
        return true;
    }

    bool ConfigManager::parse_main_section(GKeyFile *key_file, gchar *group) {
        g_autoptr(GError) error = NULL;
        g_auto(GStrv) keys = NULL;

        keys = g_key_file_get_keys(key_file, group, NULL, &error);
        g_assert_nonnull(keys);
        bool bool_value;
        for (int i = 0; keys[i] != NULL; i++) {
            gchar *key = keys[i];
            if (g_strcmp0(key, "ignore_audio") == 0) {
                if (
                    !read_bool(key_file, group, key, bool_value)
                    || !set_ignore_audio(bool_value)
                ) {
                    return false;
                }
            } else if (g_strcmp0(key, "wait_before_sleep") == 0) {
                if (
                    !read_bool(key_file, group, key, bool_value)
                    || !set_wait_before_sleep(bool_value)
                ) {
                    return false;
                }
            } else if (g_strcmp0(key, "disable_automatic_dpms_activation") == 0) {
                if (
                    !read_bool(key_file, group, key, bool_value)
                    || !set_disable_automatic_dpms_activation(bool_value)
                ) {
                    return false;
                }
            } else if (g_strcmp0(key, "disable_screensaver") == 0) {
                if (
                    !read_bool(key_file, group, key, bool_value)
                    || !set_disable_screensaver(bool_value)
                ) {
                    return false;
                }
            } else if (g_strcmp0(key, "wake_resumes_activity") == 0) {
                if (
                    !read_bool(key_file, group, key, bool_value)
                    || !set_wake_resumes_activity(bool_value)
                ) {
                    return false;
                }
            } else if (g_strcmp0(key, "enable_dbus") == 0) {
                if (
                    !read_bool(key_file, group, key, bool_value)
                    || !set_enable_dbus(bool_value)
                ) {
                    return false;
                }
            } else {
                g_warning("Unrecognized key '%s' in Main section", key);
                return false;
            }
        }
        return true;
    }

    bool ConfigManager::set_ignore_audio(bool value) {
        ignore_audio = value;
        return true;
    }
    bool ConfigManager::set_wait_before_sleep(bool value) {
        wait_before_sleep = value;
        return true;
    }
    bool ConfigManager::set_disable_automatic_dpms_activation(bool value) {
        disable_automatic_dpms_activation = value;
        return true;
    }
    bool ConfigManager::set_disable_screensaver(bool value) {
        disable_screensaver = value;
        return true;
    }
    bool ConfigManager::set_wake_resumes_activity(bool value) {
        wake_resumes_activity = value;
        return true;
    }
    bool ConfigManager::set_enable_dbus(bool value) {
        enable_dbus = value;
        return true;
    }

    gboolean ConfigManager::static_save_config_to_file(gpointer user_data) {
        ConfigManager *_this = (ConfigManager*)user_data;
        _this->save_config_to_file();
        return FALSE;
    }

    void ConfigManager::save_config_to_file() {
        if (config_file_path.empty()) {
            g_warning("config file path is not set; not saving config");
            return;
        }

        g_autoptr(GKeyFile) key_file = NULL;
        g_autoptr(GError) error = NULL;
        g_auto(GStrv) groups = NULL;

        key_file = g_key_file_new();
        if (!g_key_file_load_from_file(key_file, config_file_path.c_str(), key_file_flags, &error)) {
            if (error->code == G_FILE_ERROR_NOENT) {
                g_warning("Config file %s does not exist; creating a new file", config_file_path.c_str());
                g_error_free(error);
                error = NULL;
            } else {
                g_warning("Could not read config file %s: %s", config_file_path.c_str(), error->message);
                return;
            }
        }
        g_key_file_set_value(key_file, "Main", "ignore_audio", bool_to_str(ignore_audio));
        g_key_file_set_value(key_file, "Main", "wait_before_sleep", bool_to_str(wait_before_sleep));
        g_key_file_set_value(key_file, "Main", "disable_automatic_dpms_activation", bool_to_str(disable_automatic_dpms_activation));
        g_key_file_set_value(key_file, "Main", "disable_screensaver", bool_to_str(disable_screensaver));
        g_key_file_set_value(key_file, "Main", "wake_resumes_activity", bool_to_str(wake_resumes_activity));
        g_key_file_set_value(key_file, "Main", "enable_dbus", bool_to_str(enable_dbus));

        unordered_set<string> command_names;
        for (shared_ptr<Command> cmd : get_all_commands()) {
            command_names.insert(cmd->name);
        }

        groups = g_key_file_get_groups(key_file, NULL);
        for (int i = 0; groups[i] != NULL; i++) {
            gchar *group = groups[i];
            if (g_strcmp0(group, "Main") == 0) {
                continue;
            } else if (g_str_has_prefix(group, action_prefix)) {
                string name = string(group + action_prefix_len);
                if (command_names.find(name) == command_names.end()) {
                    // command must have been removed via DBus
                    g_key_file_remove_group(key_file, group, NULL);
                }
            } else {
                g_warning("Unexpected section name '%s'; not saving config", group);
                return;
            }
        }

        for (const shared_ptr<Command> cmd : get_all_commands()) {
            string group_name = string("Action " + cmd->name);
            gchar *trigger_str = cmd->get_trigger_str();
            g_key_file_set_value(key_file, group_name.c_str(), "trigger", trigger_str);
            g_free(trigger_str);
            if (cmd->activation_action) {
                g_key_file_set_value(key_file, group_name.c_str(), "exec", cmd->activation_action->get_cmd_str());
            } else if (g_key_file_has_key(key_file, group_name.c_str(), "exec", NULL)) {
                g_key_file_remove_key(key_file, group_name.c_str(), "exec", NULL);
            }
            if (cmd->deactivation_action) {
                g_key_file_set_value(key_file, group_name.c_str(), "resume_exec", cmd->deactivation_action->get_cmd_str());
            } else if (g_key_file_has_key(key_file, group_name.c_str(), "resume_exec", NULL)) {
                g_key_file_remove_key(key_file, group_name.c_str(), "resume_exec", NULL);
            }
        }

        g_autofree gchar *data = g_key_file_to_data(key_file, NULL, NULL);
        ofstream output(config_file_path);
        output << data;
        if (output.fail()) {
            g_warning("error saving config to file");
        }
        g_debug("saved new config to file");
    }

    void ConfigManager::save_config_to_file_async() {
        g_idle_add(static_save_config_to_file, this);
    }

    bool ConfigManager::parse_config_file(const string &filename) {
        g_autoptr(GKeyFile) key_file = NULL;
        g_autoptr(GError) error = NULL;
        g_auto(GStrv) groups = NULL;

        key_file = g_key_file_new();
        if (filename.empty()) {
            config_file_path = get_default_config_file_path();
            if (config_file_path.empty()) {
                g_warning("Could not determine config file path");
                return false;
            }
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
            if (g_strcmp0(group, "Main") == 0) {
                if (!parse_main_section(key_file, group)) {
                    return false;
                }
            } else if (g_str_has_prefix(group, action_prefix)) {
                if (!parse_action_section(key_file, group)) {
                    return false;
                }
            } else {
                g_warning("Unexpected section name '%s'", group);
                return false;
            }
        }
        return true;
    }
}
