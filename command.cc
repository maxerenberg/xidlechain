#include "command.h"

#include <cstddef>

namespace Xidlechain {
    Command::Command(): Command(NULL, NULL, 0) {}

    Command::Command(const char *before_cmd, const char *after_cmd, int64_t timeout_ms):
        activated{false},
        before_cmd{before_cmd == NULL ? "" : before_cmd},
        after_cmd{after_cmd == NULL ? "" : after_cmd},
        timeout_ms{timeout_ms}
    {}

}
