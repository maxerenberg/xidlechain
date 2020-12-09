#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include "logind_manager.h"

using std::runtime_error;

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
        if (sleep_lock_fd) {
            close(sleep_lock_fd);
        }
    }

    char *DbusLogindManager::get_session_id() {
        char *session_id = getenv("XDG_SESSION_ID");
        if (session_id && session_id[0] != '\0') {
            return strdup(session_id);
        }
        // Iterate over all sessions and try to find a session for the
        // current user which has a seat
        uid_t uid = getuid();
        gchar *seat_id;
        guint32 user_id;
        GError *err = NULL;
        GVariant *res = NULL,
                 *arr = NULL,
                 *item = NULL;
        GVariantIter iter;

        res = g_dbus_proxy_call_sync(
            manager_proxy, "ListSessions",
            NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
        if (err) throw err;
        // Take the array out of the tuple
        g_variant_get(res, "(@a(susso))", &arr);
        g_variant_unref(res);
        g_variant_iter_init(&iter, arr);
        bool found = false;
        while ((item = g_variant_iter_next_value(&iter)) != NULL) {
            g_assert(g_variant_is_of_type(item, G_VARIANT_TYPE("(susso)")));
            g_variant_get(item, "(susso)",
                          &session_id, &user_id, NULL, &seat_id, NULL);
            if (user_id == uid && seat_id[0] != '\0') {
                found = true;
                g_info("Found session ID %s", session_id);
            }
            g_free(seat_id);
            g_variant_unref(item);
            if (found) break;
            g_free(session_id);
        }
        g_variant_unref(arr);
        if (found) return session_id;
        throw runtime_error("Session ID not found");
    }

    bool DbusLogindManager::init(EventReceiver *receiver) {
        GError *err = NULL;
        GVariant *res = NULL;
        g_return_val_if_fail(receiver != NULL, FALSE);
        event_receiver = receiver;
        try {
            manager_proxy = g_dbus_proxy_new_for_bus_sync(
                G_BUS_TYPE_SYSTEM,
                G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                NULL,
                BUS_NAME,
                MANAGER_OBJECT_PATH,
                MANAGER_INTERFACE_NAME,
                NULL,
                &err);
            if (err) throw err;
            
            char *session_id = get_session_id();
            res = g_dbus_proxy_call_sync(
                manager_proxy, "GetSession",
                g_variant_new("(s)", session_id),
                G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
            g_free(session_id);
            if (err) throw err;
            gchar *session_object_path = NULL;
            g_variant_get(res, "(o)", &session_object_path);
            g_variant_unref(res);

            session_proxy = g_dbus_proxy_new_for_bus_sync(
                G_BUS_TYPE_SYSTEM,
                G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                NULL,
                BUS_NAME,
                session_object_path,
                SESSION_INTERFACE_NAME,
                NULL,
                &err);
            g_free(session_object_path);
            if (err) throw err;
            // listen for LOCK and UNLOCK signals
            g_signal_connect(session_proxy, "g-signal", 
                             G_CALLBACK(login1_session_signal_cb), this);
            // acquire an Inhibitor lock
            acquire_sleep_lock();
            // listen for SLEEP and RESUME signals
            g_signal_connect(manager_proxy, "g-signal",
                             G_CALLBACK(login1_manager_signal_cb), this);
        } catch (GError *e) {
            g_critical("%s", e->message);
            g_error_free(e);
            return false;
        } catch (runtime_error &e) {
            g_critical("%s", e.what());
            return false;
        }
        return true;
    }

    void DbusLogindManager::acquire_sleep_lock() {
        if (sleep_lock_fd >= 0) return;

        GError *err = NULL;
        GVariant *res = NULL;
        GUnixFDList *fd_list = NULL;
        gint32 fd_index = 0;

        // Apparently you have to use the unix_fd_list version to
        // actually extract the fd
        res = g_dbus_proxy_call_with_unix_fd_list_sync(
            manager_proxy, "Inhibit",
            g_variant_new("(ssss)", "sleep", APP_NAME,
                          "before-sleep and after-resume callbacks",
                          "delay"),
            G_DBUS_CALL_FLAGS_NONE, -1, NULL,
            &fd_list, NULL, &err);
        if (err) throw err;
        g_variant_get(res, "(h)", &fd_index);
        g_variant_unref(res);
        // I'm not sure why we need to duplicate the file descriptor,
        // but apparently it's necessary...
        sleep_lock_fd = g_unix_fd_list_get(fd_list, fd_index, &err);
        g_object_unref(fd_list);
        if (err) throw err;
    }

    bool DbusLogindManager::set_idle_hint(bool idle) {
        GError *err = NULL;
        g_return_val_if_fail(session_proxy != NULL, FALSE);
        g_info("Setting idle hint to %s", idle ? "true" : "false");
        try {
            g_dbus_proxy_call_sync(
                session_proxy, "SetIdleHint",
                g_variant_new("(b)", idle),
                G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
            if (err) throw err;
        } catch (GError *e) {
            // The call will fail with error message
            // "Idle hint control is not supported on non-graphical sessions"
            // if the user did not use a graphical login.
            g_warning("%s", e->message);
            g_error_free(e);
            return false;
        }
        return true;
    }

    void DbusLogindManager::login1_session_signal_cb(
            GDBusProxy *proxy, gchar *sender_name, gchar *signal_name,
            GVariant *parameters, gpointer user_data)
    {
        DbusLogindManager *_this = static_cast<DbusLogindManager*>(user_data);
        if (strcmp(signal_name, "Lock") == 0) {
            g_info("Received Lock signal");
            _this->event_receiver->receive(EVENT_LOCK, NULL);
        } else if (strcmp(signal_name, "Unlock") == 0) {
            g_info("Received Unlock signal");
            _this->event_receiver->receive(EVENT_UNLOCK, NULL);
        }
    }

    void DbusLogindManager::login1_manager_signal_cb(
            GDBusProxy *proxy, gchar *sender_name, gchar *signal_name,
            GVariant *parameters, gpointer user_data)
    {
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
            try {
                _this->acquire_sleep_lock();
            } catch (GError *e) {
                g_critical("%s", e->message);
                g_error_free(e);
            }
        }
    }
}