#ifndef _PROCESS_SPAWNER_H_
#define _PROCESS_SPAWNER_H_

namespace Xidlechain {
    class ProcessSpawner {
    public:
        // Spawns a new process for |cmd|. Blocks until the process exits.
        virtual void exec_cmd_sync(char *cmd) = 0;
        // Spawns a new process for |cmd|. Does not wait for the process to
        // exit.
        virtual void exec_cmd_async(char *cmd) = 0;
    protected:
        ~ProcessSpawner() = default;
    };

    class GProcessSpawner: public ProcessSpawner {
        void exec_cmd(char *cmd, bool wait);
    public:
        void exec_cmd_sync(char *cmd) override;
        void exec_cmd_async(char *cmd) override;
    };
}

#endif
