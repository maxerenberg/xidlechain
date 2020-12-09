#ifndef _LOGIND_MANAGER_H_
#define _LOGIND_MANAGER_H_

#include <gio/gio.h>
#include "event_receiver.h"

namespace Xidlechain {
    class LogindManager {
    public:
        // Initializes the detector and specifies the event receiver.
        // Emits the following signals (with NULL as data) upon
        // detection from logind: LOCK, UNLOCK, SLEEP, WAKE.
        virtual bool init(EventReceiver *receiver) = 0;
        // Sets the value of the "IdleHint" (see systemd-logind docs).
        virtual bool set_idle_hint(bool idle) = 0;
    protected:
        virtual ~LogindManager() = default;
    };

    class DbusLogindManager: public LogindManager {
        EventReceiver *event_receiver;
        GDBusProxy *manager_proxy,
                   *session_proxy;
        int sleep_lock_fd;
        static const char * const BUS_NAME,
                          * const MANAGER_OBJECT_PATH,
                          * const MANAGER_INTERFACE_NAME,
                          * const SESSION_INTERFACE_NAME;

        char *get_session_id();
        void acquire_sleep_lock();

        static void login1_session_signal_cb(
            GDBusProxy *proxy, gchar *sender_name, gchar *signal_name,
            GVariant *parameters, gpointer user_data);
        
        static void login1_manager_signal_cb(
            GDBusProxy *proxy, gchar *sender_name, gchar *signal_name,
            GVariant *parameters, gpointer user_data);
    public:
        DbusLogindManager();
        ~DbusLogindManager();

        bool init(EventReceiver *receiver) override;

        bool set_idle_hint(bool idle) override;
    };
}

#endif