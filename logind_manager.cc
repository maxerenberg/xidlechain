#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include "app.h"
#include "defer.h"
#include "event_receiver.h"
#include "logind_manager.h"

namespace Xidlechain {
    const char * const DbusLogindManager::BUS_NAME = "org.freedesktop.login1",
               * const DbusLogindManager::MANAGER_OBJECT_PATH = "/org/freedesktop/login1",
               * const DbusLogindManager::MANAGER_INTERFACE_NAME = "org.freedesktop.login1.Manager",
               * const DbusLogindManager::SESSION_INTERFACE_NAME = "org.freedesktop.login1.Session";

    DbusLogindManager::DbusLogindManager():
        event_receiver(NULL),
        manager_proxy(NULL),
        session_proxy(NULL),
        sleep_lock_fd(-1)
    {}

    DbusLogindManager::~DbusLogindManager() {
        if (manager_proxy) {
            g_object_unref(manager_proxy);
        }
        if (session_proxy) {
            g_object_unref(session_proxy);
        }
        if (sleep_lock_fd >= 0) {
            close(sleep_lock_fd);
        }
    }

    bool DbusLogindManager::get_session_object_path(char **result) {
        char *session_id = getenv("XDG_SESSION_ID");
        if (session_id && session_id[0] != '\0') {
            g_autoptr(GError) err = NULL;
            g_autoptr(GVariant) res = g_dbus_proxy_call_sync(
                manager_proxy,
                "GetSession",
                g_variant_new("(s)", session_id),
                G_DBUS_CALL_FLAGS_NONE,
                -1,
                NULL,
                &err);
            if (err) {
                g_warning(err->message);
                return false;
            }
            g_variant_get(res, "(o)", result);
        } else {
            *result = g_strdup("/org/freedesktop/login1/session/auto");
        }
        return true;
    }

    bool DbusLogindManager::init(EventReceiver *receiver) {
        g_return_val_if_fail(receiver != NULL, FALSE);
        event_receiver = receiver;

        GError *err = NULL;
        g_autofree gchar *session_object_path = NULL;
        Defer defer([&]{
            if (err) {
                g_warning(err->message);
                g_error_free(err);
            }
        });

        manager_proxy = g_dbus_proxy_new_for_bus_sync(
            G_BUS_TYPE_SYSTEM,
            G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
            NULL,
            BUS_NAME,
            MANAGER_OBJECT_PATH,
            MANAGER_INTERFACE_NAME,
            NULL,
            &err
        );
        if (err) return false;

        if (!get_session_object_path(&session_object_path)) return false;

        session_proxy = g_dbus_proxy_new_for_bus_sync(
            G_BUS_TYPE_SYSTEM,
            G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
            NULL,
            BUS_NAME,
            session_object_path,
            SESSION_INTERFACE_NAME,
            NULL,
            &err
        );
        if (err) return false;

        // subscribe to Lock, Unlock, and PrepareForSleep signals
        // TODO: unsubscribe from these signals in the destructor
        subscribe_to_lock_and_unlock_signals(session_object_path);
        subscribe_to_prepare_for_sleep_signal();
        // acquire an Inhibitor lock
        if (!acquire_sleep_lock()) return false;
        return true;
    }

    void DbusLogindManager::subscribe_to_prepare_for_sleep_signal() {
        GDBusConnection *manager_conn = g_dbus_proxy_get_connection(manager_proxy);
        g_dbus_connection_signal_subscribe(
            manager_conn,
            BUS_NAME,
            MANAGER_INTERFACE_NAME,
            "PrepareForSleep",
            MANAGER_OBJECT_PATH,
            NULL,
            G_DBUS_SIGNAL_FLAGS_NONE,
            login1_manager_signal_cb,
            this,
            NULL
        );
    }

    void DbusLogindManager::subscribe_to_lock_and_unlock_signals(const char *session_object_path) {
        GDBusConnection *session_conn = g_dbus_proxy_get_connection(session_proxy);
        const char *signal_names[] = {"Lock", "Unlock"};
        for (int i = 0; i < 2; i++) {
            g_dbus_connection_signal_subscribe(
                session_conn,
                BUS_NAME,
                SESSION_INTERFACE_NAME,
                signal_names[i],
                session_object_path,
                NULL,
                G_DBUS_SIGNAL_FLAGS_NONE,
                login1_session_signal_cb,
                this,
                NULL
            );
        }
    }

    bool DbusLogindManager::acquire_sleep_lock() {
        if (sleep_lock_fd >= 0) return true;

        GError *err = NULL;
        g_autoptr(GVariant) res = NULL;
        g_autoptr(GUnixFDList) fd_list = NULL;
        gint32 fd_index = 0;
        Defer defer([&]{
            if (err) {
                g_warning(err->message);
                g_error_free(err);
            }
        });

        // Apparently you have to use the unix_fd_list version to
        // actually extract the fd
        res = g_dbus_proxy_call_with_unix_fd_list_sync(
            manager_proxy,
            "Inhibit",
            g_variant_new(
                "(ssss)",
                "sleep",
                APP_NAME,
                "before-sleep and after-resume callbacks",
                "delay"
            ),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            &fd_list,
            NULL,
            &err
        );
        if (err) return false;
        g_variant_get(res, "(h)", &fd_index);
        // I'm not sure why we need to duplicate the file descriptor,
        // but apparently it's necessary...
        sleep_lock_fd = g_unix_fd_list_get(fd_list, fd_index, &err);
        if (err) return false;
        return true;
    }

    bool DbusLogindManager::set_idle_hint(bool idle) {
        g_return_val_if_fail(session_proxy != NULL, FALSE);
        GError *err = NULL;
        g_info("Setting idle hint to %s", idle ? "true" : "false");
        g_dbus_proxy_call_sync(
            session_proxy, "SetIdleHint",
            g_variant_new("(b)", idle),
            G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
        if (err) {
            // The call will fail with error message
            // "Idle hint control is not supported on non-graphical sessions"
            // if the user did not use a graphical login.
            g_warning("%s", err->message);
            g_error_free(err);
            return false;
        }
        return true;
    }

    bool DbusLogindManager::set_brightness(const char *subsystem, unsigned int value) {
        g_autoptr(GError) error = NULL;
        g_autoptr(GVariant) res = g_dbus_proxy_call_sync(
            session_proxy,
            "SetBrightness",
            g_variant_new("(ssu)", "backlight", subsystem, value),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            &error
        );
        if (error) {
            g_warning("Could not set brightness of %s to %u: %s", subsystem, value, error->message);
            return false;
        }
        return true;
    }

    bool DbusLogindManager::suspend() {
        g_autoptr(GError) error = NULL;
        g_autoptr(GVariant) res = g_dbus_proxy_call_sync(
            manager_proxy,
            "Suspend",
            g_variant_new("(b)", FALSE),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            &error
        );
        if (error) {
            g_warning("Could not suspend: %s", error->message);
            return false;
        }
        return true;
    }

    void DbusLogindManager::login1_session_signal_cb(
        GDBusConnection *connection,
        const gchar *sender_name,
        const gchar *object_path,
        const gchar *interface_name,
        const gchar *signal_name,
        GVariant *parameters,
        gpointer user_data
    ) {
        DbusLogindManager *_this = static_cast<DbusLogindManager*>(user_data);
        if (strcmp(signal_name, "Lock") == 0) {
            g_debug("Received Lock signal");
            _this->event_receiver->receive(EVENT_LOCK, NULL);
        } else if (strcmp(signal_name, "Unlock") == 0) {
            g_debug("Received Unlock signal");
            _this->event_receiver->receive(EVENT_UNLOCK, NULL);
        }
    }

    void DbusLogindManager::login1_manager_signal_cb(
        GDBusConnection *connection,
        const gchar *sender_name,
        const gchar *object_path,
        const gchar *interface_name,
        const gchar *signal_name,
        GVariant *parameters,
        gpointer user_data
    ) {
        DbusLogindManager *_this = static_cast<DbusLogindManager*>(user_data);
        if (strcmp(signal_name, "PrepareForSleep") != 0) {
            return;
        }
        gboolean preparing_for_sleep;
        g_variant_get(parameters, "(b)", &preparing_for_sleep);
        if (preparing_for_sleep) {
            g_info("Preparing for sleep");
            _this->event_receiver->receive(EVENT_SLEEP, NULL);
            // close the Inhibitor lock to let systemd know that we're done
            close(_this->sleep_lock_fd);
            _this->sleep_lock_fd = -1;
        } else {
            g_info("Waking up from sleep");
            _this->event_receiver->receive(EVENT_WAKE, NULL);
            _this->acquire_sleep_lock();
        }
    }
}
