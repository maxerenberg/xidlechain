#include <cstdlib>
#include <sstream>

#include <glib.h>

#include "brightness_controller.h"
#include "command.h"
#include "errors.h"
#include "logind_manager.h"
#include "process_spawner.h"

using std::abort;
using std::char_traits;
using std::istringstream;
using std::make_unique;
using std::string;
using std::unique_ptr;

static bool read_int_from_string(const char *s, int &result) {
    istringstream iss(s);
    iss >> result;
    return !iss.fail();
}

namespace Xidlechain {
    unique_ptr<Command::Action> Command::Action::factory(const char *cmd_str, GError **error) {
        static constexpr const char * const builtin_cmd_prefix = "builtin:";
        static constexpr const int builtin_cmd_prefix_len = char_traits<char>::length(builtin_cmd_prefix);
        if (cmd_str[0] == '\0') {
            // empty string
            return unique_ptr<Command::Action>(nullptr);
        }
        if (g_str_has_prefix(cmd_str, builtin_cmd_prefix)) {
            cmd_str = cmd_str + builtin_cmd_prefix_len;
            if (g_strcmp0(cmd_str, "dim") == 0) {
                return make_unique<Command::DimAction>();
            } else if (g_strcmp0(cmd_str, "undim") == 0) {
                return make_unique<Command::UndimAction>();
            } else if (g_strcmp0(cmd_str, "suspend") == 0) {
                return make_unique<Command::SuspendAction>();
            } else if (g_strcmp0(cmd_str, "set_idle_hint") == 0) {
                return make_unique<Command::SetIdleHintAction>();
            } else if (g_strcmp0(cmd_str, "unset_idle_hint") == 0) {
                return make_unique<Command::UnsetIdleHintAction>();
            } else {
                g_set_error(error, XIDLECHAIN_ERROR, XIDLECHAIN_ERROR_INVALID_ACTION, "Unknown builtin '%s'", cmd_str);
                return unique_ptr<Command::Action>(nullptr);
            }
        }
        return make_unique<Command::ShellAction>(cmd_str);
    }

    bool Command::is_valid(GError **error) const {
        if (trigger == Command::NONE) {
            g_set_error(error, XIDLECHAIN_ERROR, XIDLECHAIN_ERROR_MISSING_TRIGGER, "Action '%s' must specify a trigger type", name.c_str());
            return false;
        }
        if (!activation_action && !deactivation_action) {
            g_set_error(error, XIDLECHAIN_ERROR, XIDLECHAIN_ERROR_MISSING_ACTION, "Action '%s' must specify either exec or resume_exec", name.c_str());
            return false;
        }
        return true;

    }

    Command::ShellAction::ShellAction(const char *cmd_arg): cmd{cmd_arg} {
        if (cmd.empty()) {
            g_warning("shell command may not be empty");
            abort();
        }
    }

    const char *Command::ShellAction::get_cmd_str() const {
        return cmd.c_str();
    }

    bool Command::ShellAction::execute(const Command::ActionExecutors &executors) {
        ProcessSpawner *process_spawner = executors.process_spawner;
        process_spawner->exec_cmd_async(cmd);
        return true;
    }

    bool Command::ShellAction::execute_sync(const Command::ActionExecutors &executors) {
        ProcessSpawner *process_spawner = executors.process_spawner;
        process_spawner->exec_cmd_sync(cmd);
        return true;
    }

    bool Command::DimAction::execute(const Command::ActionExecutors &executors) {
        BrightnessController *brightness_controller = executors.brightness_controller;
        return brightness_controller->dim();
    }

    const char *Command::DimAction::get_cmd_str() const {
        return "builtin:dim";
    }

    bool Command::UndimAction::execute(const Command::ActionExecutors &executors) {
        BrightnessController *brightness_controller = executors.brightness_controller;
        brightness_controller->restore_brightness();
        return true;
    }

    const char *Command::UndimAction::get_cmd_str() const {
        return "builtin:undim";
    }

    bool Command::SuspendAction::execute(const Command::ActionExecutors &executors) {
        LogindManager *logind_manager = executors.logind_manager;
        return logind_manager->suspend();
    }

    const char *Command::SuspendAction::get_cmd_str() const {
        return "builtin:suspend";
    }

    bool Command::SetIdleHintAction::execute(const Command::ActionExecutors &executors) {
        LogindManager *logind_manager = executors.logind_manager;
        return logind_manager->set_idle_hint(true);
    }

    const char *Command::SetIdleHintAction::get_cmd_str() const {
        return "builtin:set_idle_hint";
    }

    bool Command::UnsetIdleHintAction::execute(const Command::ActionExecutors &executors) {
        LogindManager *logind_manager = executors.logind_manager;
        return logind_manager->set_idle_hint(false);
    }

    const char *Command::UnsetIdleHintAction::get_cmd_str() const {
        return "builtin:unset_idle_hint";
    }

    Command::Command():
        id{0},
        trigger{NONE},
        timeout_ms{0},
        activated{false}
    {}

    char *Command::static_get_trigger_str(Trigger trigger, int timeout_ms) {
        switch (trigger) {
        case TIMEOUT:
            return g_strdup_printf("timeout %d", (int)(timeout_ms / 1000));
        case SLEEP:
            return g_strdup("sleep");
        case LOCK:
            return g_strdup("lock");
        default:
            g_critical("Trigger not set");
            abort();
        }
    }

    char *Command::get_trigger_str() const {
        return static_get_trigger_str(trigger, timeout_ms);
    }

    bool Command::set_trigger_from_str(const char *val, GError **error) {
        static constexpr const char * const timeout_prefix = "timeout ";
        static constexpr const int timeout_prefix_len = char_traits<char>::length(timeout_prefix);

        if (g_str_has_prefix(val, timeout_prefix)) {
            int timeout_sec;
            if (!read_int_from_string(val + timeout_prefix_len, timeout_sec)) {
                g_set_error(error, XIDLECHAIN_ERROR, XIDLECHAIN_ERROR_INVALID_TRIGGER, "Timeout value must be an integer");
                return false;
            }
            if (timeout_sec <= 0) {
                g_set_error(error, XIDLECHAIN_ERROR, XIDLECHAIN_ERROR_INVALID_TRIGGER, "Timeout must be positive");
                return false;
            }
            trigger = Command::TIMEOUT;
            timeout_ms = (int64_t)timeout_sec * 1000;
        } else if (g_strcmp0(val, "sleep") == 0) {
            trigger = Command::SLEEP;
        } else if (g_strcmp0(val, "lock") == 0) {
            trigger = Command::LOCK;
        } else {
            g_set_error(error, XIDLECHAIN_ERROR, XIDLECHAIN_ERROR_INVALID_TRIGGER, "Unrecognized trigger type '%s'", val);
            return false;
        }
        return true;
    }

    void Command::activate(const Command::ActionExecutors &executors, bool sync) {
        if (trigger == TIMEOUT) {
            if (activated) return;
            activated = true;
        }
        if (!activation_action) return;
        if (sync) {
            activation_action->execute_sync(executors);
        } else {
            activation_action->execute(executors);
        }
    }

    void Command::deactivate(const Command::ActionExecutors &executors, bool sync) {
        // TODO: kill activation_action process if it's still running
        if (trigger == TIMEOUT) {
            if (!activated) return;
            activated = false;
        }
        if (!deactivation_action) return;
        if (sync) {
            deactivation_action->execute_sync(executors);
        } else {
            deactivation_action->execute(executors);
        }
    }

    bool Command::is_activated() const {
        return activated;
    }
}
