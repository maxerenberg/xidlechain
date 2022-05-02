#ifndef _BRIGHTNESS_CONTROLLER_H_
#define _BRIGHTNESS_CONTROLLER_H_

#include <string>

#include <gio/gio.h>
#include <gudev/gudev.h>

using std::string;

namespace Xidlechain {
    class LogindManager;

    class BrightnessController {
    public:
        virtual bool init(LogindManager *logind_manager) = 0;
        virtual bool dim() = 0;
        virtual void restore_brightness() = 0;
    protected:
        virtual ~BrightnessController() = default;
    };

    class DbusBrightnessController: public BrightnessController {
        // Possible transitions:
        // NONE -> DIMMING
        // DIMMING -> CANCELLED
        // DIMMING -> DIMMED
        // CANCELLED -> DIMMED
        // DIMMED -> NONE
        enum DimmingState {
            NONE,
            DIMMING,
            CANCELLED,
            DIMMED
        };
        DimmingState dimming_state;
        int original_brightness;
        LogindManager *logind_manager;
        GUdevClient *udev_client;
        GUdevDevice *backlight_device;
        GCancellable *dimmer_task_cancellable;

        bool get_backlight_device();
        int get_current_brightness();

        void dimmer_task_func(GTask *task);
        static void static_dimmer_task_func(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable);
        void dimmer_task_complete_cb();
        static void static_dimmer_task_complete_cb(GObject *source_object, GAsyncResult *res, gpointer user_data);
    public:
        DbusBrightnessController();
        ~DbusBrightnessController();

        bool init(LogindManager *logind_manager) override;
        bool dim() override;
        void restore_brightness() override;
    };
}

#endif
