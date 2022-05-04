#include <cstdlib>
#include <vector>

#include <gio/gio.h>

#include "app.h"
#include "config_manager.h"
#include "dbus_request_handler.h"
#include "event_receiver.h"
#include "xidlechain_action_generated.h"
#include "xidlechain_generated.h"

using std::abort;
using std::vector;

namespace Xidlechain {
    void DbusRequestHandler::on_bus_acquired(
        GDBusConnection *connection,
        const gchar *name
    ) {
        CXidlechain *config_iface = c_xidlechain_skeleton_new();
        c_xidlechain_set_ignore_audio(config_iface, cfg->ignore_audio);
        c_xidlechain_set_wait_before_sleep(config_iface, cfg->wait_before_sleep);
        c_xidlechain_set_idle_hint_timeout_sec(config_iface, cfg->idlehint_timeout_sec);
        c_xidlechain_set_disable_automatic_dpmsactivation(config_iface, cfg->disable_automatic_dpms_activation);
        c_xidlechain_set_disable_screensaver(config_iface, cfg->disable_screensaver);
        g_autoptr(GError) error = NULL;
        if (!g_dbus_interface_skeleton_export(
            G_DBUS_INTERFACE_SKELETON(config_iface),
            connection,
            DBUS_OBJECT_BASE_PATH,
            &error
        )) {
            g_warning(error->message);
            g_object_unref(config_iface);
            return;
        }
        // config_iface doesn't get unref'd

        g_autofree gchar *object_manager_path = g_strdup_printf("%s/action", DBUS_OBJECT_BASE_PATH);
        GDBusObjectManagerServer *manager = g_dbus_object_manager_server_new(object_manager_path);
        add_action_objects_from_list(manager, cfg->timeout_commands);
        add_action_objects_from_list(manager, cfg->sleep_commands);
        add_action_objects_from_list(manager, cfg->lock_commands);
        g_dbus_object_manager_server_set_connection(manager, connection);
        // manager doesn't get unref'd
    }

    void DbusRequestHandler::static_on_bus_acquired(
        GDBusConnection *connection,
        const gchar *name,
        gpointer user_data
    ) {
        g_debug("Acquired a bus connection");
        DbusRequestHandler *_this = (DbusRequestHandler*)user_data;
        _this->on_bus_acquired(connection, name);
    }

    void DbusRequestHandler::static_on_name_lost(
        GDBusConnection *connection,
        const gchar *name,
        gpointer user_data
    ) {
        if (connection == NULL) {
            g_critical("Connection to bus could not be made");
        } else {
            g_critical("Could not acquire name %s", name);
        }
        abort();
    }

    void DbusRequestHandler::static_on_name_acquired(
        GDBusConnection *connection,
        const gchar *name,
        gpointer user_data
    ) {
        g_debug("Acquired name %s", name);
    }

    void DbusRequestHandler::add_action_objects_from_list(GDBusObjectManagerServer *manager, vector<Command> &list) {
        static int counter = 0;
        // An object called 'Action' on the DBus interface is actually
        // called 'Command' in our code.
        // Maybe we should rename this...
        for (Command &cmd : list) {
            int id = ++counter;
            gchar *object_path = g_strdup_printf("%s/action/%d", DBUS_OBJECT_BASE_PATH, id);
            CObjectSkeleton *object = c_object_skeleton_new(object_path);
            g_free(object_path);
            CXidlechainAction *action = c_xidlechain_action_skeleton_new();
            c_xidlechain_action_set_name(action, cmd.name.c_str());
            g_autofree gchar *trigger = cmd.get_trigger_str();
            c_xidlechain_action_set_trigger(action, trigger);
            if (cmd.activation_action) {
                c_xidlechain_action_set_exec(action, cmd.activation_action->get_cmd_str());
            }
            if (cmd.deactivation_action) {
                c_xidlechain_action_set_resume_exec(action, cmd.deactivation_action->get_cmd_str());
            }
            c_object_skeleton_set_xidlechain_action(object, action);
            g_object_unref(action);
            g_dbus_object_manager_server_export(manager, G_DBUS_OBJECT_SKELETON(object));
            g_object_unref(object);
            action_id_to_command[id] = &cmd;
        }
    }

    void DbusRequestHandler::init(
        ConfigManager *config_manager,
        EventReceiver *event_receiver
    ) {
        cfg = config_manager;
        this->event_receiver = event_receiver;
        bus_identifier = g_bus_own_name(
            G_BUS_TYPE_SESSION,
            DBUS_BUS_NAME,
            G_BUS_NAME_OWNER_FLAGS_NONE,
            static_on_bus_acquired,
            static_on_name_acquired,
            static_on_name_lost,
            this,
            NULL
        );
    }
}
