#include <csignal>
#include <cstdlib>
#include <sstream>
#include <gdk/gdk.h>
#include "event_manager.h"

using namespace std;

class ShowUsage {};

static int parse_timeout(int argc, char **argv, Xidlechain::EventManager &manager) {
    if (argc < 3) {
        g_critical("Too few parameters to timeout command. "
                   "Usage: timeout <seconds> <command>");
        exit(1);
    }
    istringstream iss(argv[1]);
    int64_t timeout_sec;
    iss >> timeout_sec;
    if (!iss || timeout_sec < 1) {
        g_critical("Invalid value for timeout");
        exit(1);
    }
    char *before_cmd = argv[2],
         *after_cmd = NULL;
    int args_read = 3;
    if (argc >= 5 && strcmp(argv[3], "resume") == 0) {
        after_cmd = argv[4];
        args_read = 5;
    }
    manager.add_timeout_command(before_cmd, after_cmd, timeout_sec*1000);
    return args_read;
}

static int parse_one_cmd(int argc, char **argv, Xidlechain::EventType event_type,
                         Xidlechain::EventManager &manager)
{
    if (argc < 2) {
        g_critical("Too few parameters for %s command", argv[0]);
        exit(1);
    }
    switch (event_type) {
        case Xidlechain::EVENT_LOCK:
            manager.set_lock_cmd(argv[1]);
            break;
        case Xidlechain::EVENT_UNLOCK:
            manager.set_unlock_cmd(argv[1]);
            break;
        case Xidlechain::EVENT_SLEEP:
            manager.set_sleep_cmd(argv[1]);
            break;
        case Xidlechain::EVENT_WAKE:
            manager.set_resume_cmd(argv[1]);
            break;
        default:
            g_critical("Unrecognized event type %d", event_type);
            exit(1);
    }
    return 2;
}

static int parse_idlehint(int argc, char **argv, Xidlechain::EventManager &manager) {
    if (argc < 2) {
        g_critical("Too few parameters for idlehint command. "
                   "Usage: idlehint <timeout>");
        exit(1);
    }
    int64_t timeout_sec;
    istringstream iss(argv[1]);
    iss >> timeout_sec;
    if (!iss || timeout_sec < 1) {
        g_critical("Invalid value for timeout");
        exit(1);
    }
    manager.enable_idle_hint(timeout_sec*1000);
    return 2;
}

static bool ignore_sigchld() {
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = SIG_IGN;
    if (sigaction(SIGCHLD, &act, NULL) != 0) {
        perror("sigaction");
        return false;
    }
    return true;
}

int main(int argc, char *argv[]) {
    int ch;
    bool should_wait = false;
    Xidlechain::EventManager event_manager;

    gdk_init(&argc, &argv);

    try {
        while ((ch = getopt(argc, argv, "hdw")) != -1) {
            switch (ch) {
                case 'd':
                    putenv("G_MESSAGES_DEBUG=all");
                    break;
                case 'w':
                    should_wait = true;
                    break;
                case 'h':
                case '?':
                default:
                    throw ShowUsage();
            }
        }
    } catch (ShowUsage&) {
        printf("Usage: %s [OPTIONS]\n"
               "  -h\tthis help menu\n"
               "  -d\tdebug\n"
               "  -w\twait for command to finish\n",
               argv[0]);
        return ch == 'h' ? 0 : 1;
    }

    if (!event_manager.init()) {
        return 1;
    }
    event_manager.set_should_wait(should_wait);

    int i = optind;
    while (i < argc) {
        if (strcmp("timeout", argv[i]) == 0) {
            i += parse_timeout(argc-i, argv+i, event_manager);
        } else if (strcmp("lock", argv[i]) == 0) {
            i += parse_one_cmd(argc-i, argv+i, Xidlechain::EVENT_LOCK,
                               event_manager);
        } else if (strcmp("unlock", argv[i]) == 0) {
            i += parse_one_cmd(argc-i, argv+i, Xidlechain::EVENT_UNLOCK,
                               event_manager);
        } else if (strcmp("before-sleep", argv[i]) == 0) {
            i += parse_one_cmd(argc-i, argv+i, Xidlechain::EVENT_SLEEP,
                               event_manager);
        } else if (strcmp("after-resume", argv[i]) == 0) {
            i += parse_one_cmd(argc-i, argv+i, Xidlechain::EVENT_WAKE,
                               event_manager);
        } else if (strcmp("idlehint", argv[i]) == 0) {
            i += parse_idlehint(argc-i, argv+i, event_manager);
        } else {
            g_critical("Unsupported command '%s'", argv[i]);
            return 1;
        }
    }

    // prevent child processes from turning into zombies
    if (!ignore_sigchld()) {
        return 1;
    }

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    return 0;
}
