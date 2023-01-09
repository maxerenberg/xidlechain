#include "brightness_controller.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <sstream>
#include <unistd.h>  // nanosleep

#include <gudev/gudev.h>

#include "logind_manager.h"

using std::int64_t;
using std::istringstream;
using std::strerror;
using std::timespec;

static const int64_t NANOSEC_PER_SEC      = 1000000000;
static const int64_t MILLISEC_PER_SEC     =       1000;
static const int64_t NANOSEC_PER_MILLISEC =    1000000;

static void add_nanoseconds_to_timespec(const timespec *in_ts, int64_t nanosecs, timespec *out_ts) {
    out_ts->tv_sec = in_ts->tv_sec;
    out_ts->tv_nsec = in_ts->tv_nsec + nanosecs;
    out_ts->tv_sec += out_ts->tv_nsec / NANOSEC_PER_SEC;
    out_ts->tv_nsec %= NANOSEC_PER_SEC;
}

static int timespec_cmp(const timespec *a, const timespec *b) {
    if (a->tv_sec < b->tv_sec) return -1;
    if (a->tv_sec > b->tv_sec) return 1;
    if (a->tv_nsec < b->tv_nsec) return -1;
    if (a->tv_nsec > b->tv_nsec) return 1;
    return 0;
}

static int timespec_diff_in_millis(const timespec *a, const timespec *b) {
    int sec_diff = a->tv_sec - b->tv_sec;
    int64_t nsec_diff = a->tv_nsec - b->tv_nsec;
    return sec_diff * MILLISEC_PER_SEC + nsec_diff / NANOSEC_PER_MILLISEC;
}

namespace Xidlechain {
    DbusBrightnessController::DbusBrightnessController():
        dimming_state{NONE},
        original_brightness{0},
        logind_manager{NULL},
        udev_client{NULL},
        backlight_device{NULL},
        dimmer_task_cancellable{NULL}
    {
        const gchar * const subsystems[] = {"backlight", NULL};
        udev_client = g_udev_client_new(subsystems);
    }

    DbusBrightnessController::~DbusBrightnessController() {
        g_object_unref(udev_client);
        if (backlight_device) {
            g_object_unref(backlight_device);
        }
    }

    static int get_device_type_idx(const gchar * const device_types[], GUdevDevice *device) {
        const gchar *device_type = g_udev_device_get_sysfs_attr(device, "type");
        int i;
        for (i = 0; device_types[i] != NULL; i++) {
            if (g_strcmp0(device_types[i], device_type) == 0) {
                return i;
            }
        }
        g_warning("Device '%s' has unknown type '%s'", g_udev_device_get_name(device), device_type);
        return i;
    }

    bool DbusBrightnessController::get_backlight_device() {
        // When multiple backlight
	// interfaces are available for a single device, firmware
	// control should be preferred to platform control should
	// be preferred to raw control.
        //
        // Source: https://www.kernel.org/doc/Documentation/ABI/stable/sysfs-class-backlight
        const gchar * const device_types[] = {"firmware", "platform", "raw", NULL};

        g_autolist(GUdevDevice) devices = g_udev_client_query_by_subsystem(udev_client, "backlight");
        GUdevDevice *best_device = NULL;
        int best_device_type_idx = 3;
        for (GList *item = devices; item != NULL; item = item->next) {
            GUdevDevice *device = (GUdevDevice*) item->data;
            int device_type_idx = get_device_type_idx(device_types, device);
            if (device_type_idx < best_device_type_idx) {
                best_device = device;
                best_device_type_idx = device_type_idx;
            }
        }
        if (best_device == NULL) {
            g_warning("Could not find backlight device");
            return false;
        }
        backlight_device = best_device;
        g_object_ref(backlight_device);
        g_info("Using backlight device %s", g_udev_device_get_name(best_device));
        return true;
    }

    int DbusBrightnessController::get_current_brightness() {
        const gchar *brightness_str = g_udev_device_get_sysfs_attr(backlight_device, "brightness");
        g_assert_nonnull(brightness_str);
        istringstream iss(brightness_str);
        int brightness;
        iss >> brightness;
        g_assert(!iss.fail());
        return brightness;
    }

    bool DbusBrightnessController::init(LogindManager *logind_manager) {
        g_assert_nonnull(logind_manager);
        this->logind_manager = logind_manager;
        return get_backlight_device();
    }

    void DbusBrightnessController::dimmer_task_func(GTask *task) {
        g_debug("Starting to dim from brightness = %d", original_brightness);
        // Maybe we should make these configurable?
        const int step_millis = 100;
        const int64_t total_millis = 5000;
        timespec now;
        // TODO: use dynamic delay to prevent clock drift
        const timespec delay = {
            .tv_sec = 0,
            .tv_nsec = step_millis * NANOSEC_PER_MILLISEC
        };
        const char * const device_name = g_udev_device_get_name(backlight_device);
        if (clock_gettime(CLOCK_BOOTTIME, &now) < 0) {
            g_warning("clock_gettime: %s", strerror(errno));
            g_task_return_pointer(task, NULL, NULL);
            return;
        }
        timespec start_time = now;
        timespec target_time;
        add_nanoseconds_to_timespec(&now, total_millis * NANOSEC_PER_MILLISEC, &target_time);
        int cur_brightness = original_brightness;
        // On laptops using intel_backlight, the firmware/BIOS will set the
        // brightness to 100% after resuming from sleep if it was at 0 before
        // entering sleep. So we don't want to go all the way down to 0.
        // See https://bbs.archlinux.org/viewtopic.php?id=231909.
        const int min_brightness =
            (g_strcmp0(g_udev_device_get_name(backlight_device), "intel_backlight") == 0) ? 1 : 0;
        if (cur_brightness <= min_brightness) {
            // Nothing to do
            g_task_return_pointer(task, NULL, NULL);
            return;
        }
        const int total_delta = cur_brightness - min_brightness;
        // Gradually decrease the brightness until it is min_brightness
        while (timespec_cmp(&now, &target_time) < 0) {
            if (!g_task_set_return_on_cancel(task, FALSE)) {
                g_debug("dimmer task got cancelled");
                g_task_return_pointer(task, NULL, NULL);
                return;
            }

            nanosleep(&delay, NULL);
            clock_gettime(CLOCK_BOOTTIME, &now);
            int millis_elapsed = timespec_diff_in_millis(&now, &start_time);
            if (millis_elapsed >= total_millis) break;
            // next = (1 - (millis_elapsed / total_millis)) * total_delta
            //      = total_delta - (millis_elapsed * total_delta) / total_millis
            int next_brightness = (int)(total_delta - ((int64_t)millis_elapsed * total_delta) / total_millis);
            if (next_brightness != cur_brightness) {
                logind_manager->set_brightness(device_name, (unsigned)next_brightness);
                cur_brightness = next_brightness;
            }

            g_task_set_return_on_cancel(task, TRUE);
        }
        if (cur_brightness > min_brightness) {
            logind_manager->set_brightness(device_name, (unsigned)min_brightness);
        }
        g_task_return_pointer(task, NULL, NULL);
    }

    void DbusBrightnessController::static_dimmer_task_func(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
        DbusBrightnessController *_this = static_cast<DbusBrightnessController*>(task_data);
        _this->dimmer_task_func(task);
    }

    void DbusBrightnessController::dimmer_task_complete_cb() {
        g_assert(
            dimming_state == DIMMING
            || dimming_state == CANCELLED
        );
        if (dimming_state == CANCELLED) {
            g_debug("Restoring original brightness in callback to %d", original_brightness);
            logind_manager->set_brightness(g_udev_device_get_name(backlight_device), original_brightness);
            // We're skipping the transition to DIMMED and going straight to NONE
            dimming_state = NONE;
        } else {
            dimming_state = DIMMED;
        }
        g_object_unref(dimmer_task_cancellable);
        dimmer_task_cancellable = NULL;
    }

    void DbusBrightnessController::static_dimmer_task_complete_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
        g_debug("dimmer task completed");
        DbusBrightnessController *_this = static_cast<DbusBrightnessController*>(user_data);
        _this->dimmer_task_complete_cb();
    }

    bool DbusBrightnessController::dim() {
        if (dimming_state != NONE) {
            g_warning("already dimming or dimmed");
            return false;
        }
        dimming_state = DIMMING;
        original_brightness = get_current_brightness();
        dimmer_task_cancellable = g_cancellable_new();
        GTask *task = g_task_new(NULL, dimmer_task_cancellable, static_dimmer_task_complete_cb, this);
        g_task_set_task_data(task, this, NULL);
        g_task_run_in_thread(task, static_dimmer_task_func);
        g_object_unref(task);
        return true;
    }

    void DbusBrightnessController::restore_brightness() {
        switch (dimming_state) {
        case NONE:
            g_info("Cannot restore brightness because monitor is not dimmed or dimming");
            break;
        case DIMMING:
            dimming_state = CANCELLED;
            g_cancellable_cancel(dimmer_task_cancellable);
            break;
        case CANCELLED:
            // Nothing to do
            break;
        case DIMMED:
            g_debug("Restoring original brightness to %d", original_brightness);
            logind_manager->set_brightness(g_udev_device_get_name(backlight_device), original_brightness);
            dimming_state = NONE;
            break;
        }
    }
}
