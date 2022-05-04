#include <cstdlib>

#include <glib.h>

#include "brightness_controller.h"
#include "command.h"
#include "logind_manager.h"
#include "process_spawner.h"

using std::abort;
using std::char_traits;
using std::make_unique;
using std::string;
using std::unique_ptr;

namespace Xidlechain {
    unique_ptr<Command::Action> Command::Action::factory(const char *cmd_str) {
        static constexpr const char * const builtin_cmd_prefix = "builtin:";
        static constexpr const int builtin_cmd_prefix_len = char_traits<char>::length(builtin_cmd_prefix);
        if (g_str_has_prefix(cmd_str, builtin_cmd_prefix)) {
            cmd_str = cmd_str + builtin_cmd_prefix_len;
            if (g_strcmp0(cmd_str, "dim") == 0) {
                return make_unique<Command::DimAction>();
            } else if (g_strcmp0(cmd_str, "undim") == 0) {
                return make_unique<Command::UndimAction>();
            } else if (g_strcmp0(cmd_str, "suspend") == 0) {
                return make_unique<Command::SuspendAction>();
            } else {
                g_warning("Unknown builtin '%s'", cmd_str);
                abort();
            }
        }
        return make_unique<Command::ShellAction>(cmd_str);
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

    Command::Command():
        trigger{NONE},
        timeout_ms{0},
        activated{false}
    {}

    char *Command::get_trigger_str() const {
        switch (trigger) {
        case Command::TIMEOUT:
            return g_strdup_printf("timeout %d", (int)(timeout_ms / 1000));
        case Command::SLEEP:
            return g_strdup("sleep");
        case Command::LOCK:
            return g_strdup("lock");
        default:
            g_critical("Trigger not set");
            abort();
        }
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
