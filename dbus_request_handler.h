#ifndef _DBUS_REQUEST_HANDLER_H_
#define _DBUS_REQUEST_HANDLER_H_

#include <memory>
#include <vector>

#include <gio/gio.h>

#include "command.h"

using std::shared_ptr;
using std::vector;

struct _CXidlechain;

namespace Xidlechain {
    class ConfigManager;
    class EventReceiver;

    class DbusRequestHandler {
        ConfigManager *cfg = nullptr;
        EventReceiver *event_receiver = nullptr;
        guint bus_identifier = 0;
        GDBusObjectManagerServer *object_manager = nullptr;
        // There'll only be one instance of this class for the lifetime
        // of the program
        static DbusRequestHandler *INSTANCE;

        static gboolean static_set_property_func(
            GDBusConnection *connection,
            const gchar *sender,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *property_name,
            GVariant *value,
            GError **error,
            gpointer user_data
        );
        gboolean handle_set_property(
            const gchar *object_path,
            const gchar *property_name,
            GVariant *value,
            GError **error
        );
        static gboolean static_set_property_func_for_action(
            GDBusConnection *connection,
            const gchar *sender,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *property_name,
            GVariant *value,
            GError **error,
            gpointer user_data
        );
        gboolean handle_set_property_for_action(
            const gchar *object_path,
            const gchar *property_name,
            GVariant *value,
            GError **error
        );
        static gboolean static_on_add_action(
            _CXidlechain *object,
            GDBusMethodInvocation *invocation,
            const gchar *name,
            const gchar *trigger,
            const gchar *exec,
            const gchar *resume_exec,
            gpointer user_data
        );
        void on_add_action(
            _CXidlechain *object,
            GDBusMethodInvocation *invocation,
            const gchar *name,
            const gchar *trigger,
            const gchar *exec,
            const gchar *resume_exec
        );
        static gboolean static_on_remove_action(
            _CXidlechain *object,
            GDBusMethodInvocation *invocation,
            gint id,
            gpointer user_data
        );
        void on_remove_action(
            _CXidlechain *object,
            GDBusMethodInvocation *invocation,
            gint id
        );

        static void static_on_bus_acquired(
            GDBusConnection *connection,
            const gchar *name,
            gpointer user_data
        );
        void on_bus_acquired(
            GDBusConnection *connection,
            const gchar *name
        );
        static void static_on_name_acquired(
            GDBusConnection *connection,
            const gchar *name,
            gpointer user_data
        );
        static void static_on_name_lost(
            GDBusConnection *connection,
            const gchar *name,
            gpointer user_data
        );
        void add_action_to_object_manager(Command &cmd);
    public:
        void init(
            ConfigManager *config_manager,
            EventReceiver *event_receiver
        );
    };
}

#endif
