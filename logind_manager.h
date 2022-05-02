#ifndef _LOGIND_MANAGER_H_
#define _LOGIND_MANAGER_H_

#include <gio/gio.h>

namespace Xidlechain {
    class EventReceiver;

    class LogindManager {
    public:
        // Initializes the detector and specifies the event receiver.
        // Emits the following signals (with NULL as data) upon
        // detection from logind: LOCK, UNLOCK, SLEEP, WAKE.
        virtual bool init(EventReceiver *receiver) = 0;
        // Sets the value of the "IdleHint" (see systemd-logind docs).
        virtual bool set_idle_hint(bool idle) = 0;
        virtual bool set_brightness(const char *subsystem, unsigned int value) = 0;
        virtual bool suspend() = 0;
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

        bool get_session_id(char **result);
        void subscribe_to_lock_and_unlock_signals(const char *session_object_path);
        void subscribe_to_prepare_for_sleep_signal();
        bool acquire_sleep_lock();

        static void login1_session_signal_cb(
            GDBusConnection *connection,
            const gchar *sender_name,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *signal_name,
            GVariant *parameters,
            gpointer user_data
        );

        static void login1_manager_signal_cb(
            GDBusConnection *connection,
            const gchar *sender_name,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *signal_name,
            GVariant *parameters,
            gpointer user_data
        );
    public:
        DbusLogindManager();
        ~DbusLogindManager();

        bool init(EventReceiver *receiver) override;
        bool set_idle_hint(bool idle) override;
        bool set_brightness(const char *subsystem, unsigned int value) override;
        bool suspend() override;
    };
}

#endif
