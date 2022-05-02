#include <csignal>
#include <cstdlib>
#include <getopt.h>
#include <sstream>
#include <gdk/gdk.h>
#include "activity_detector.h"
#include "audio_detector.h"
#include "brightness_controller.h"
#include "config_manager.h"
#include "event_manager.h"
#include "logind_manager.h"
#include "process_spawner.h"

using namespace std;

class ShowUsage {};

int main(int argc, char *argv[]) {
    int ch;
    string config_file_path;

    try {
        while ((ch = getopt(argc, argv, "c:dh")) != -1) {
            switch (ch) {
                case 'c':
                    config_file_path = string(optarg);
                    break;
                case 'd':
                    setenv("G_MESSAGES_DEBUG", "all", 1);
                    break;
                case 'h':
                case '?':
                default:
                    throw ShowUsage();
            }
        }
        if (optind != argc) {
            throw ShowUsage();
        }
    } catch (ShowUsage&) {
        printf("Usage: %s [OPTIONS]\n"
               "  -h\tthis help menu\n"
               "  -d\tdebug\n"
               "  -c\tconfiguration file path\n",
               argv[0]);
        return ch == 'h' ? 0 : 1;
    }

    // GDK parses its own arguments - maybe it would be better to pass
    // a dummy array here instead?
    // https://gitlab.gnome.org/GNOME/gtk/-/blob/gtk-3-0/gdk/gdk.c#L181
    gdk_init(&argc, &argv);

    Xidlechain::ConfigManager config_manager;
    if (!config_manager.parse_config_file(config_file_path)) {
        return EXIT_FAILURE;
    }

    Xidlechain::EventManager event_manager(&config_manager);
    Xidlechain::XsyncActivityDetector activity_detector;
    Xidlechain::PulseAudioDetector audio_detector;
    Xidlechain::DbusLogindManager logind_manager;
    Xidlechain::GProcessSpawner process_spawner;
    Xidlechain::DbusBrightnessController brightness_controller;
    bool success = event_manager.init(&activity_detector,
                                      &logind_manager,
                                      &audio_detector,
                                      &process_spawner,
                                      &brightness_controller);
    if (!success) return EXIT_FAILURE;

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    return 0;
}
