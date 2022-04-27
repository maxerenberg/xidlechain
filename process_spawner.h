#ifndef _PROCESS_SPAWNER_H_
#define _PROCESS_SPAWNER_H_

#include <string>

using std::string;

namespace Xidlechain {
    class ProcessSpawner {
    public:
        // Spawns a new process for |cmd|. Blocks until the process exits.
        virtual void exec_cmd_sync(const string &cmd) = 0;
        // Spawns a new process for |cmd|. Does not wait for the process to
        // exit.
        virtual void exec_cmd_async(const string &cmd) = 0;
    protected:
        ~ProcessSpawner() = default;
    };

    class GProcessSpawner: public ProcessSpawner {
        void exec_cmd(const string &cmd, bool wait);
    public:
        void exec_cmd_sync(const string &cmd) override;
        void exec_cmd_async(const string &cmd) override;
    };
}

#endif
