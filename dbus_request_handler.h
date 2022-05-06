#ifndef _DBUS_REQUEST_HANDLER_H_
#define _DBUS_REQUEST_HANDLER_H_

#include <unordered_map>
#include <vector>

#include <gio/gio.h>

#include "command.h"

using std::unordered_map;
using std::vector;

namespace Xidlechain {
    class ConfigManager;
    class EventReceiver;

    class DbusRequestHandler {
        ConfigManager *cfg = nullptr;
        EventReceiver *event_receiver = nullptr;
        guint bus_identifier = 0;
        unordered_map<int, Command*> action_id_to_command;
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
        void add_action_objects_from_list(GDBusObjectManagerServer *manager, vector<Command> &list);
    public:
        void init(
            ConfigManager *config_manager,
            EventReceiver *event_receiver
        );
    };
}

#endif
