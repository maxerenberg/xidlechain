// Adapted from here:
// https://chromium.googlesource.com/chromiumos/platform/power_manager/+/refs/heads/0.12.433.B62/xidle.h

#ifndef _ACTIVITY_DETECTOR_H_
#define _ACTIVITY_DETECTOR_H_

#include <unordered_map>
#include <gdk/gdk.h>
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>
#include "event_receiver.h"

using std::unordered_map;

namespace Xidlechain {
    class ActivityDetector {
    public:
        // Initializes the detector and specifies the event receiver.
        virtual bool init(EventReceiver *receiver) = 0;
        // After |timeout_ms| of inactivity, an ACTIVITY_TIMEOUT event will
        // be emitted with |data| as the data parameter. If activity resumes,
        // an ACTIVITY_RESUME event will be emitted with NULL for the data.
        virtual bool add_idle_timeout(int64_t timeout_ms, gpointer data) = 0;
        // Deletes all timers.
        virtual bool clear_timeouts() = 0;
    protected:
        virtual ~ActivityDetector() = default;
    };

    class XsyncActivityDetector: public ActivityDetector {
        XSyncCounter idle_counter_id;
        int sync_event_base,
            sync_error_base;
        // This is the smallest timeout out of all of our pos_trans_alarms.
        int64_t min_timeout;
        // These alarms trigger when a user becomes inactive for a certain
        // period of time.
        unordered_map<XSyncAlarm, gpointer> pos_trans_alarms;
        // This alarms triggers when a user used to be inactive for a certain
        // period of time, then became active again.
        XSyncAlarm neg_trans_alarm;
        EventReceiver *event_receiver;
        Display *xdisplay;

        XSyncAlarm create_idle_alarm(int64_t timeout_ms, XSyncTestType test_type);

        static GdkFilterReturn gdk_event_filter(
            GdkXEvent *gxevent, GdkEvent *gevent, gpointer data);
    public:
        XsyncActivityDetector();
        ~XsyncActivityDetector();
        XsyncActivityDetector(const XsyncActivityDetector&) = delete;
        XsyncActivityDetector& operator=(const XsyncActivityDetector&) = delete;

        bool init(EventReceiver *receiver) override;

        bool add_idle_timeout(int64_t timeout_ms, gpointer data) override;

        bool clear_timeouts() override;
    };
}

#endif