#include "config_manager.h"

#include <string>
#include <utility>

#include "defer.h"

using std::string;

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

static void read_string(GKeyFile *key_file, gchar *group, gchar *key, string &result, GError *error) {
    gchar *val = g_key_file_get_value(key_file, group, key, &error);
    g_assert_nonnull(val);
    result = string(val);
    g_free(val);
}

namespace Xidlechain {
    bool ConfigManager::parse_action_section(GKeyFile *key_file, gchar *group) {
        GError *error = NULL;
        gchar **keys = NULL;
        gchar *action_name = group + action_prefix_len;
        Defer defer([&]{
            if (error != NULL) g_error_free(error);
            if (keys != NULL) g_strfreev(keys);
        });

        keys = g_key_file_get_keys(key_file, group, NULL, &error);
        g_assert_nonnull(keys);
        Command cmd{};
        for (int i = 0; keys[i] != NULL; i++) {
            gchar *key = keys[i];
            if (g_strcmp0(key, "timeout") == 0) {
                int timeout_sec = -1;
                if (!read_int(key_file, group, key, timeout_sec, error)) {
                    return false;
                }
                if (timeout_sec <= 0) {
                    g_warning("Timeout for action %s must be positive", action_name);
                    return false;
                }
                cmd.timeout_ms = (int64_t)timeout_sec * 1000;
            } else if (g_strcmp0(key, "exec") == 0) {
                read_string(key_file, group, key, cmd.before_cmd, error);
            } else if (g_strcmp0(key, "resume_exec") == 0) {
                read_string(key_file, group, key, cmd.after_cmd, error);
            } else {
                g_warning("Unrecognized key '%s' in section %s", key, group);
                return false;
            }
        }
        if (cmd.timeout_ms == 0) {
            g_warning("Timeout for action '%s' must be specified", action_name);
            return false;
        } else if (cmd.before_cmd == "") {
            g_warning("Action '%s' must have a command to execute", action_name);
            return false;
        }
        activity_commands.emplace_back(std::move(cmd));
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
            } else if (g_strcmp0(key, "before_sleep_exec") == 0) {
                read_string(key_file, group, key, sleep_cmd.before_cmd, error);
            } else if (g_strcmp0(key, "after_wake_exec") == 0) {
                read_string(key_file, group, key, sleep_cmd.after_cmd, error);
            } else if (g_strcmp0(key, "lock_exec") == 0) {
                read_string(key_file, group, key, lock_cmd.before_cmd, error);
            } else if (g_strcmp0(key, "unlock_exec") == 0) {
                read_string(key_file, group, key, lock_cmd.after_cmd, error);
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
