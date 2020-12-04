// Adapted from here:
// https://chromium.googlesource.com/chromiumos/platform/power_manager/+/refs/heads/0.12.433.B62/xidle.cc

#include <gdk/gdkx.h>
#include <glib.h>
#include <limits>
#include "activity_manager.h"

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
    ActivityManager::ActivityManager():
        idle_counter_id(0),
        min_timeout(numeric_limits<int64_t>::max()),
        neg_trans_alarm(None),
        event_receiver(NULL)
    {}

    ActivityManager::~ActivityManager() {
        clear_timeouts();
    }

    bool ActivityManager::init(EventReceiver *receiver) {
        g_return_val_if_fail(receiver != NULL, FALSE);
        g_return_val_if_fail(GDK_DISPLAY() != NULL, FALSE);
        event_receiver = receiver;
        int major_version, minor_version;
        if (!(XSyncQueryExtension(GDK_DISPLAY(), &sync_event_base, &sync_error_base) &&
              XSyncInitialize(GDK_DISPLAY(), &major_version, &minor_version)))
        {
            g_critical("Failed to initialize XSync extension");
            return false;
        }
        int ncounters;
        XSyncSystemCounter *counters = XSyncListSystemCounters(GDK_DISPLAY(), &ncounters);
        if (counters) {
            for (int i = 0; i < ncounters; i++) {
                if (counters[i].name && strcmp(counters[i].name, "IDLETIME") == 0) {
                    idle_counter_id = counters[i].counter;
                    break;
                }
            }
            XSyncFreeSystemCounterList(counters);
            if (idle_counter_id) {
                gdk_window_add_filter(NULL, gdk_event_filter, this);
            }
        }
        if (idle_counter_id == 0) {
            g_critical("IDLETIME system sync counter not found");
            return false;
        }
        return true;
    }

    bool ActivityManager::add_idle_timeout(int64_t timeout_ms, gpointer data) {
        g_return_val_if_fail(idle_counter_id != 0, FALSE);
        g_return_val_if_fail(timeout_ms > 1, FALSE);

        if (timeout_ms < min_timeout) {
            min_timeout = timeout_ms;
            // Setup an alarm to fire when the user was idle, but is now active.
            // This occurs when IDLETIME > min_timeout - 1, and the user becomes
            // active.
            XSyncAlarm alarm = create_idle_alarm(min_timeout-1, XSyncNegativeTransition);
            g_return_val_if_fail(alarm != None, FALSE);
            if (neg_trans_alarm) {
                XSyncDestroyAlarm(GDK_DISPLAY(), neg_trans_alarm);
            }
            neg_trans_alarm = alarm;
        }
        // Send idle event when IDLETIME >= timeout_ms
        XSyncAlarm alarm = create_idle_alarm(timeout_ms, XSyncPositiveTransition);
        g_return_val_if_fail(alarm != None, FALSE);
        pos_trans_alarms[alarm] = data;
        return true;
    }

    XSyncAlarm ActivityManager::create_idle_alarm(int64_t timeout_ms,
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
        return XSyncCreateAlarm(GDK_DISPLAY(), mask, &attr);
    }

    bool ActivityManager::clear_timeouts() {
        for (const pair<XSyncAlarm, gpointer> &p : pos_trans_alarms) {
            XSyncDestroyAlarm(GDK_DISPLAY(), p.first);
        }
        pos_trans_alarms.clear();
        if (neg_trans_alarm) {
            XSyncDestroyAlarm(GDK_DISPLAY(), neg_trans_alarm);
            neg_trans_alarm = None;
        }
        min_timeout = numeric_limits<int64_t>::max();
        return true;
    }

    GdkFilterReturn ActivityManager::gdk_event_filter(
        GdkXEvent *gxevent, GdkEvent *gevent, gpointer data)
    {
        XEvent* xevent = static_cast<XEvent*>(gxevent);
        XSyncAlarmNotifyEvent* alarm_event =
            static_cast<XSyncAlarmNotifyEvent*>(gxevent);
        ActivityManager* _this = static_cast<ActivityManager*>(data);
        XSyncValue value;

        if (xevent->type == _this->sync_event_base + XSyncAlarmNotify &&
            alarm_event->state != XSyncAlarmDestroyed &&
            XSyncQueryCounter(GDK_DISPLAY(), _this->idle_counter_id, &value))
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
                user_data = _this->pos_trans_alarms[alarm_event->alarm];
            } else {
                g_info("Activity resume at %ld ms", idle_time_ms);
                event_type = EVENT_ACTIVITY_RESUME;
                user_data = NULL;
            }
            _this->event_receiver->receive(event_type, user_data);
        }
        return GDK_FILTER_CONTINUE;
    }
}