#ifndef _PROCESS_SPAWNER_H_
#define _PROCESS_SPAWNER_H_

#include <unordered_set>
#include <glib.h>

using std::unordered_set;

namespace Xidlechain {
    class ProcessSpawner {
    public:
        // Spawns a new process for |cmd|. Blocks until the process exits.
        virtual void exec_cmd_sync(char *cmd) = 0;
        // Spawns a new process for |cmd|. Does not wait for the process to
        // exit. Child PID is stored so that it may be killed later.
        virtual void exec_cmd_async(char *cmd) = 0;
        // Kills all lingering child processes.
        virtual void kill_children() = 0;
    protected:
        ~ProcessSpawner() = default;
    };

    class GProcessSpawner: public ProcessSpawner {
        unordered_set<GPid> children;

        void exec_cmd(char *cmd, bool wait);
        static void child_watch_cb(GPid pid, gint status, gpointer data);
    public:
        void exec_cmd_sync(char *cmd) override;
        void exec_cmd_async(char *cmd) override;
        void kill_children() override;
    };
}

#endif