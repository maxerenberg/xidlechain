// Adapted from here:
// https://chromium.googlesource.com/chromiumos/platform/power_manager/+/refs/heads/0.12.433.B62/xidle.h

#ifndef _ACTIVITY_MANAGER_H_
#define _ACTIVITY_MANAGER_H_

#include <unordered_map>
#include <gdk/gdkevents.h>
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>
#include "event_receiver.h"

using std::unordered_map;

namespace Xidlechain {
    class ActivityManager {
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

        XSyncAlarm create_idle_alarm(int64_t timeout_ms, XSyncTestType test_type);

        static GdkFilterReturn gdk_event_filter(
            GdkXEvent *gxevent, GdkEvent *gevent, gpointer data);
    public:
        ActivityManager();
        ~ActivityManager();
        ActivityManager(const ActivityManager&) = delete;
        ActivityManager& operator=(const ActivityManager&) = delete;

        bool init(EventReceiver *receiver);

        bool add_idle_timeout(int64_t timeout_ms, gpointer data);

        bool clear_timeouts();
    };
}

#endif