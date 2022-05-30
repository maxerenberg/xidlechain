// Adapted from here:
// https://chromium.googlesource.com/chromiumos/platform/power_manager/+/refs/heads/0.12.433.B62/xidle.cc

/* TODO:
 * Create a single alarm with a wait_value of 1ms, and use it to detect when
 * the user becomes active. Keep track of the timestamp of when the alarm
 * most recently triggered.
 * This appears to be the approach used by Mutter, the window manager for
 * GNOME (see
 * https://gitlab.gnome.org/GNOME/mutter/-/blob/main/src/backends/x11/meta-backend-x11.c).
 * This will allow us to "reset" the idle counter when the system wakes
 * from sleep.
 */

#include <gdk/gdkx.h>
#include <glib.h>
#include <limits>
#include "activity_detector.h"

using std::numeric_limits;
using std::pair;

static inline int64_t XSyncValueToInt64(XSyncValue value) {
    return (static_cast<int64_t>(XSyncValueHigh32(value)) << 32 |
            static_cast<int64_t>(XSyncValueLow32(value)));
}

static inline void XSyncInt64ToValue(XSyncValue* xvalue, int64_t value) {
    XSyncIntsToValue(xvalue, value, value >> 32);
}

namespace Xidlechain {
    XsyncActivityDetector::XsyncActivityDetector():
        idle_counter_id(0),
        min_timeout(numeric_limits<int64_t>::max()),
        neg_trans_alarm(None),
        event_receiver(NULL),
        xdisplay(NULL)
    {}

    XsyncActivityDetector::~XsyncActivityDetector() {
        clear_timeouts();
    }

    bool XsyncActivityDetector::init(EventReceiver *receiver) {
        g_return_val_if_fail(receiver != NULL, FALSE);
        event_receiver = receiver;

        xdisplay = gdk_x11_get_default_xdisplay();
        g_return_val_if_fail(xdisplay != NULL, FALSE);

        int major_version, minor_version;
        if (!(XSyncQueryExtension(xdisplay, &sync_event_base, &sync_error_base) &&
              XSyncInitialize(xdisplay, &major_version, &minor_version)))
        {
            g_critical("Failed to initialize XSync extension");
            return false;
        }
        int ncounters;
        XSyncSystemCounter *counters = XSyncListSystemCounters(xdisplay, &ncounters);
        if (counters) {
            for (int i = 0; i < ncounters; i++) {
                if (counters[i].name && strcmp(counters[i].name, "IDLETIME") == 0) {
                    idle_counter_id = counters[i].counter;
                    break;
                }
            }
            XSyncFreeSystemCounterList(counters);
            if (idle_counter_id) {
                gdk_window_add_filter(NULL, static_gdk_event_filter, this);
            }
        }
        if (idle_counter_id == 0) {
            g_critical("IDLETIME system sync counter not found");
            return false;
        }
        return true;
    }

    bool XsyncActivityDetector::add_idle_timeout(int64_t timeout_ms, gpointer data) {
        g_assert(idle_counter_id != 0);
        g_assert(timeout_ms > 1);
        g_assert(pos_trans_alarms_by_data.find(data) == pos_trans_alarms_by_data.end());

        if (timeout_ms < min_timeout) {
            min_timeout = timeout_ms;
            // Setup an alarm to fire when the user was idle, but is now active.
            // This occurs when IDLETIME > min_timeout - 1, and the user becomes
            // active.
            XSyncAlarm alarm = create_idle_alarm(min_timeout-1, XSyncNegativeTransition);
            g_return_val_if_fail(alarm != None, FALSE);
            if (neg_trans_alarm) {
                XSyncDestroyAlarm(xdisplay, neg_trans_alarm);
            }
            neg_trans_alarm = alarm;
        }
        // Send idle event when IDLETIME >= timeout_ms
        XSyncAlarm alarm = create_idle_alarm(timeout_ms, XSyncPositiveTransition);
        g_return_val_if_fail(alarm != None, FALSE);
        pos_trans_alarms[alarm] = data;
        pos_trans_alarms_by_data[data] = alarm;
        return true;
    }

    bool XsyncActivityDetector::remove_idle_timeout(gpointer data) {
        unordered_map<gpointer, XSyncAlarm>::iterator it = pos_trans_alarms_by_data.find(data);
        if (it == pos_trans_alarms_by_data.end()) {
            return false;
        }
        XSyncAlarm alarm = it->second;
        XSyncDestroyAlarm(xdisplay, alarm);
        pos_trans_alarms.erase(alarm);
        pos_trans_alarms_by_data.erase(it);
        return true;
    }

    XSyncAlarm XsyncActivityDetector::create_idle_alarm(int64_t timeout_ms,
                                                  XSyncTestType test_type)
    {
        uint64_t mask = XSyncCACounter |
                        XSyncCAValue |
                        XSyncCATestType |
                        XSyncCADelta;
        XSyncAlarmAttributes attr;
        attr.trigger.counter = idle_counter_id;
        attr.trigger.test_type = test_type;
        XSyncInt64ToValue(&attr.trigger.wait_value, timeout_ms);
        XSyncIntToValue(&attr.delta, 0);
        return XSyncCreateAlarm(xdisplay, mask, &attr);
    }

    bool XsyncActivityDetector::clear_timeouts() {
        for (const pair<const XSyncAlarm, gpointer> &p : pos_trans_alarms) {
            XSyncDestroyAlarm(xdisplay, p.first);
        }
        pos_trans_alarms.clear();
        pos_trans_alarms_by_data.clear();
        if (neg_trans_alarm) {
            XSyncDestroyAlarm(xdisplay, neg_trans_alarm);
            neg_trans_alarm = None;
        }
        min_timeout = numeric_limits<int64_t>::max();
        return true;
    }

    GdkFilterReturn XsyncActivityDetector::gdk_event_filter(
        GdkXEvent *gxevent, GdkEvent *gevent)
    {
        XEvent* xevent = static_cast<XEvent*>(gxevent);
        XSyncAlarmNotifyEvent* alarm_event =
            static_cast<XSyncAlarmNotifyEvent*>(gxevent);
        XSyncValue value;

        if (xevent->type == sync_event_base + XSyncAlarmNotify &&
            alarm_event->state != XSyncAlarmDestroyed &&
            XSyncQueryCounter(xdisplay, idle_counter_id, &value))
        {
            bool is_idle = !XSyncValueLessThan(alarm_event->counter_value,
                                               alarm_event->alarm_value);
            bool is_idle2 = !XSyncValueLessThan(value,
                                                alarm_event->alarm_value);
            int64_t idle_time_ms = XSyncValueToInt64(alarm_event->counter_value);
            if (is_idle != is_idle2) {
                g_warning("Received stale event");
            }
            // We deliver the stale event anyways because the client might
            // be expecting events to be delivered in a specific order.
            EventType event_type;
            gpointer user_data;
            if (is_idle) {
                g_info("Activity timeout at %ld ms", idle_time_ms);
                event_type = EVENT_ACTIVITY_TIMEOUT;
                unordered_map<XSyncAlarm, gpointer>::iterator it = pos_trans_alarms.find(alarm_event->alarm);
                if (it == pos_trans_alarms.end()) {
                    g_warning("Received event for deleted alarm");
                    return GDK_FILTER_CONTINUE;
                }
                user_data = it->second;
            } else {
                g_info("Activity resume at %ld ms", idle_time_ms);
                event_type = EVENT_ACTIVITY_RESUME;
                user_data = NULL;
            }
            event_receiver->receive(event_type, user_data);
        }
        return GDK_FILTER_CONTINUE;
    }

    GdkFilterReturn XsyncActivityDetector::static_gdk_event_filter(
        GdkXEvent *gxevent, GdkEvent *gevent, gpointer data)
    {
        XsyncActivityDetector* _this = static_cast<XsyncActivityDetector*>(data);
        return _this->gdk_event_filter(gxevent, gevent);
    }
}
