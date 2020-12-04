#ifndef _LOGIND_MANAGER_H_
#define _LOGIND_MANAGER_H_

#include <gio/gio.h>
#include "event_receiver.h"

namespace Xidlechain {
    class LogindManager {
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
        LogindManager();
        ~LogindManager();

        bool init(EventReceiver *receiver);

        bool set_idle_hint(bool idle);
    };
}

#endif