#ifndef _COMMAND_H_
#define _COMMAND_H_

#include <cstdint>
#include <string>

using std::string;

namespace Xidlechain {
    struct Command {
        bool activated;
        string before_cmd;
        string after_cmd;
        int64_t timeout_ms;
        Command();
        Command(const char *before_cmd, const char *after_cmd, int64_t timeout_ms);
    };
}

#endif
