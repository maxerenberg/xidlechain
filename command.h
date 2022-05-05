#ifndef _COMMAND_H_
#define _COMMAND_H_

#include <cstdint>
#include <memory>
#include <string>

using std::int64_t;
using std::string;
using std::unique_ptr;

namespace Xidlechain {
    class BrightnessController;
    class ProcessSpawner;
    class LogindManager;

    class Command {
    public:
        struct ActionExecutors {
            BrightnessController *brightness_controller;
            ProcessSpawner *process_spawner;
            LogindManager *logind_manager;
        };

        class Action {
        public:
            static unique_ptr<Action> factory(const char *cmd_str);
            virtual const char *get_cmd_str() const = 0;
            // async by default
            virtual bool execute(const ActionExecutors &executors) = 0;
            virtual bool execute_sync(const ActionExecutors &executors) {
                return execute(executors);
            }
            virtual ~Action() = default;
        };

        class ShellAction: public Action {
            string cmd;
        public:
            ShellAction(const char *cmd);
            const char *get_cmd_str() const override;
            bool execute(const ActionExecutors &executors) override;
            bool execute_sync(const ActionExecutors &executors) override;
        };

        class DimAction: public Action {
        public:
            const char *get_cmd_str() const override;
            bool execute(const ActionExecutors &executors) override;
        };

        class UndimAction: public Action {
        public:
            const char *get_cmd_str() const override;
            bool execute(const ActionExecutors &executors) override;
        };

        class SuspendAction: public Action {
        public:
            const char *get_cmd_str() const override;
            bool execute(const ActionExecutors &executors) override;
        };

        class SetIdleHintAction: public Action {
        public:
            const char *get_cmd_str() const override;
            bool execute(const ActionExecutors &executors) override;
        };

        class UnsetIdleHintAction: public Action {
        public:
            const char *get_cmd_str() const override;
            bool execute(const ActionExecutors &executors) override;
        };

        enum Trigger {
            NONE,
            TIMEOUT,
            SLEEP,
            LOCK
        };

        string name;
        Trigger trigger;
        // Only used for TIMEOUT commands
        int64_t timeout_ms;
        unique_ptr<Action> activation_action;
        unique_ptr<Action> deactivation_action;

        Command();
        void activate(const ActionExecutors &executors, bool sync=false);
        void deactivate(const ActionExecutors &executors, bool sync=false);
        bool is_activated() const;
        char *get_trigger_str() const;
    private:
        // Only used for TIMEOUT commands
        // We need this because when activity is detected, we need to
        // run the deactivation actions only for the commands which have
        // been activated.
        bool activated;
    };
}

#endif
