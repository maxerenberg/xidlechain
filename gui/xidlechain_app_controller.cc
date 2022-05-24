#include <cstdint>
#include <cstdio>
#include <sstream>
#include <utility>

#include "app.h"
#include "xidlechain_app_controller.h"
#include "xidlechain_app_edit_action.h"
#include "xidlechain_app_window.h"

using std::istringstream;
using std::make_pair;
using std::make_shared;
using std::ostringstream;
using std::shared_ptr;
using std::sscanf;
using std::string;
using std::uintptr_t;
using std::unordered_map;
using Xidlechain::Command;

XidlechainAppController *XidlechainAppController::INSTANCE = nullptr;

static const char *trigger_type_to_str(Command::Trigger trigger)
{
    switch (trigger) {
    case Command::TIMEOUT:
        return "timeout";
    case Command::SLEEP:
        return "sleep";
    case Command::LOCK:
        return "lock";
    default:
        g_critical("Invalid trigger type %d", (int)trigger);
        g_abort();
    }
}

static Command::Trigger str_to_trigger_type(const char *val) {
    if (g_strcmp0(val, "timeout") == 0) {
        return Command::TIMEOUT;
    } else if (g_strcmp0(val, "sleep") == 0) {
        return Command::SLEEP;
    } else if (g_strcmp0(val, "lock") == 0) {
        return Command::LOCK;
    }
    return Command::NONE;
}

static void show_error_dialog(GtkWindow *parent, const char *message) {
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new (NULL,
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     message);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}

bool XidlechainAppController::set_action_property(
    GDBusProxy *action_proxy,
    const char *property_name,
    const char *value,
    string &error_message
) {
    g_autoptr(GError) err = NULL;
    g_autoptr(GVariant) res = g_dbus_proxy_call_sync(
        action_proxy,
        "org.freedesktop.DBus.Properties.Set",
        g_variant_new("(ssv)",
                      Xidlechain::DBUS_ACTION_IFACE,
                      property_name,
                      g_variant_new_string(value)),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &err
    );
    if (err) {
        error_message = string(err->message);
        return false;
    }
    return true;
}

void XidlechainAppController::static_on_action_edit_clicked(GtkButton*, gpointer user_data) {
    INSTANCE->on_action_edit_clicked(user_data);
}

void XidlechainAppController::on_action_edit_clicked(gpointer user_data) {
    int action_id = reinterpret_cast<uintptr_t>(user_data);
    unordered_map<int, ActionInfo>::iterator it = id_to_action_info.find(action_id);
    g_assert(it != id_to_action_info.end());
    shared_ptr<Command> cmd = it->second.cmd;

    g_assert(app != nullptr);
    GtkWindow *win = gtk_application_get_active_window(GTK_APPLICATION(app));
    XidlechainAppEditAction *dialog = xidlechain_app_edit_action_new(XIDLECHAIN_APP_WINDOW(win));
    GtkWidget *name_entry = xidlechain_app_edit_action_get_name_entry(dialog);
    gtk_entry_set_text(GTK_ENTRY(name_entry), cmd->name.c_str());
    GtkWidget *trigger_combobox = xidlechain_app_edit_action_get_trigger_combobox(dialog);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(trigger_combobox), trigger_type_to_str(cmd->trigger));
    GtkWidget *seconds_entry = xidlechain_app_edit_action_get_seconds_entry(dialog);
    if (cmd->trigger == Command::TIMEOUT) {
        ostringstream oss;
        oss << (cmd->timeout_ms / 1000);
        gtk_entry_set_text(GTK_ENTRY(seconds_entry), oss.str().c_str());
    }
    GtkWidget *activate_entry = xidlechain_app_edit_action_get_activate_entry(dialog);
    if (cmd->activation_action) {
        gtk_entry_set_text(GTK_ENTRY(activate_entry), cmd->activation_action->get_cmd_str());
    }
    GtkWidget *deactivate_entry = xidlechain_app_edit_action_get_deactivate_entry(dialog);
    if (cmd->deactivation_action) {
        gtk_entry_set_text(GTK_ENTRY(deactivate_entry), cmd->deactivation_action->get_cmd_str());
    }
    while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        if (on_action_edited(dialog, cmd)) break;
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

bool XidlechainAppController::on_action_edited(XidlechainAppEditAction *dialog, shared_ptr<Command> cmd) {
    g_autoptr(GError) error = NULL;
    string error_message;
    int timeout_sec = 0;
    GtkWidget *name_entry = xidlechain_app_edit_action_get_name_entry(dialog);
    GtkWidget *trigger_combobox = xidlechain_app_edit_action_get_trigger_combobox(dialog);
    GtkWidget *seconds_entry = xidlechain_app_edit_action_get_seconds_entry(dialog);
    GtkWidget *activate_entry = xidlechain_app_edit_action_get_activate_entry(dialog);
    GtkWidget *deactivate_entry = xidlechain_app_edit_action_get_deactivate_entry(dialog);
    Command::Trigger trigger = str_to_trigger_type(
        gtk_combo_box_get_active_id(GTK_COMBO_BOX(trigger_combobox))
    );
    if (trigger == Command::NONE) {
        show_error_dialog(GTK_WINDOW(dialog), "invalid trigger");
        return false;
    }
    if (trigger == Command::TIMEOUT) {
        istringstream iss(gtk_entry_get_text(GTK_ENTRY(seconds_entry)));
        if (!(iss >> timeout_sec) || !iss.eof()) {
            show_error_dialog(GTK_WINDOW(dialog), "seconds must be a positive integer");
            return false;
        }
    }
    g_autofree gchar *object_path = g_strdup_printf("%s/action/%d", Xidlechain::DBUS_OBJECT_BASE_PATH, cmd->id);
    g_autoptr(GDBusProxy) action_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
        NULL,
        Xidlechain::DBUS_BUS_NAME,
        object_path,
        "org.freedesktop.DBus.Properties",
        NULL,
        &error
    );
    if (error) {
        show_error_dialog(GTK_WINDOW(dialog), error->message);
        return false;
    }

    const char *name = gtk_entry_get_text(GTK_ENTRY(name_entry));
    if (g_strcmp0(name, cmd->name.c_str()) != 0) {
        if (!set_action_property(action_proxy, "Name", name, error_message)) {
            show_error_dialog(GTK_WINDOW(dialog), error_message.c_str());
            return false;
        }
        cmd->name = string(name);
        GtkWidget *name_label = id_to_action_info.find(cmd->id)->second.name_label;
        gtk_label_set_text(GTK_LABEL(name_label), name);
    }

    g_autofree gchar *trigger_str = NULL;
    int timeout_ms = timeout_sec * 1000;
    if (trigger != cmd->trigger || (trigger == Command::TIMEOUT && timeout_ms != cmd->timeout_ms)) {
        trigger_str = Command::static_get_trigger_str(trigger, timeout_ms);
        if (!set_action_property(action_proxy, "Trigger", trigger_str, error_message)) {
            show_error_dialog(GTK_WINDOW(dialog), error_message.c_str());
            return false;
        }
        cmd->trigger = trigger;
        if (trigger == Command::TIMEOUT) {
            cmd->timeout_ms = timeout_ms;
        }
    }

    const char *activate_str = gtk_entry_get_text(GTK_ENTRY(activate_entry));
    if (
        (activate_str[0] != '\0' && !cmd->activation_action)
        || (
            cmd->activation_action
            && g_strcmp0(activate_str, cmd->activation_action->get_cmd_str()) != 0
        )
    ) {
        if (!set_action_property(action_proxy, "Exec", activate_str, error_message)) {

            show_error_dialog(GTK_WINDOW(dialog), error_message.c_str());
            return false;
        }
        cmd->activation_action = Command::Action::factory(activate_str, &error);
        g_assert(error == NULL);
    }

    const char *deactivate_str = gtk_entry_get_text(GTK_ENTRY(deactivate_entry));
    if (
        (deactivate_str[0] != '\0' && !cmd->deactivation_action)
        || (
            cmd->deactivation_action
            && g_strcmp0(deactivate_str, cmd->deactivation_action->get_cmd_str()) != 0
        )
    ) {
        if (!set_action_property(action_proxy, "ResumeExec", deactivate_str, error_message)) {

            show_error_dialog(GTK_WINDOW(dialog), error_message.c_str());
            return false;
        }
        cmd->deactivation_action = Command::Action::factory(deactivate_str, &error);
        g_assert(error == NULL);
    }

    return true;
}

void XidlechainAppController::static_on_action_delete_clicked(GtkButton*, gpointer user_data) {
    INSTANCE->on_action_delete_clicked(user_data);
}

void XidlechainAppController::on_action_delete_clicked(gpointer user_data) {
    int action_id = reinterpret_cast<uintptr_t>(user_data);
    unordered_map<int, ActionInfo>::iterator it = id_to_action_info.find(action_id);
    g_assert(it != id_to_action_info.end());
    ActionInfo &action_info = it->second;
    g_autoptr(GError) error = NULL;
    XidlechainAppWindow *win = xidlechain_app_get_window(app);
    g_autoptr(GVariant) res = g_dbus_proxy_call_sync(
        proxy,
        "RemoveAction",
        g_variant_new("(i)", action_id),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );
    if (error) {
        show_error_dialog(GTK_WINDOW(win), error->message);
        return;
    }
    // From the documentation for gkt_container_remove:
    // "If you don’t want to use widget again it’s usually more efficient to
    //  simply destroy it directly using gtk_widget_destroy()"
    gtk_widget_destroy(action_info.row);
    id_to_action_info.erase(it);
}

void XidlechainAppController::static_on_pause_changed(GtkWidget *widget, gpointer data) {
    INSTANCE->on_pause_changed(widget);
}

void XidlechainAppController::on_pause_changed(GtkWidget *widget) {
    bool paused = gtk_switch_get_active(GTK_SWITCH(widget));
    const char *method = paused ? "Pause" : "Unpause";
    g_autoptr(GError) error = NULL;
    g_autoptr(GVariant) res = g_dbus_proxy_call_sync(
        proxy,
        method,
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );
    if (error) {
        GtkWindow *win = gtk_application_get_active_window(GTK_APPLICATION(app));
        show_error_dialog(win, error->message);
        // TODO: revert old value
    }
}

void XidlechainAppController::static_on_add_action_clicked(GtkButton *button, gpointer) {
    INSTANCE->on_add_action_clicked();
}

void XidlechainAppController::on_add_action_clicked() {
    g_assert(app != nullptr);
    GtkWindow *win = gtk_application_get_active_window(GTK_APPLICATION(app));
    // Re-use the "Edit Action" dialog
    XidlechainAppEditAction *dialog = xidlechain_app_edit_action_new(XIDLECHAIN_APP_WINDOW(win));
    gtk_window_set_title(GTK_WINDOW(dialog), "Add action");
    // Choose a default trigger
    GtkWidget *trigger_combobox = xidlechain_app_edit_action_get_trigger_combobox(dialog);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(trigger_combobox), "timeout");
    while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        if (on_action_added(dialog)) break;
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

bool XidlechainAppController::on_action_added(XidlechainAppEditAction *dialog) {
    // TODO: reduce code duplication with on_action_edited
    g_autoptr(GError) error = NULL;
    string error_message;
    int timeout_sec = 0;
    GtkWidget *name_entry = xidlechain_app_edit_action_get_name_entry(dialog);
    GtkWidget *trigger_combobox = xidlechain_app_edit_action_get_trigger_combobox(dialog);
    GtkWidget *seconds_entry = xidlechain_app_edit_action_get_seconds_entry(dialog);
    GtkWidget *activate_entry = xidlechain_app_edit_action_get_activate_entry(dialog);
    GtkWidget *deactivate_entry = xidlechain_app_edit_action_get_deactivate_entry(dialog);
    Command::Trigger trigger = str_to_trigger_type(
        gtk_combo_box_get_active_id(GTK_COMBO_BOX(trigger_combobox))
    );
    if (trigger == Command::NONE) {
        show_error_dialog(GTK_WINDOW(dialog), "invalid trigger");
        return false;
    }
    if (trigger == Command::TIMEOUT) {
        istringstream iss(gtk_entry_get_text(GTK_ENTRY(seconds_entry)));
        if (!(iss >> timeout_sec) || !iss.eof()) {
            show_error_dialog(GTK_WINDOW(dialog), "seconds must be a positive integer");
            return false;
        }
    }
    shared_ptr<Command> cmd = make_shared<Command>();
    cmd->name = string(gtk_entry_get_text(GTK_ENTRY(name_entry)));
    if (cmd->name.empty()) {
        show_error_dialog(GTK_WINDOW(dialog), "name must not be empty");
        return false;
    }
    cmd->trigger = trigger;
    cmd->timeout_ms = timeout_sec * 1000;
    cmd->activation_action = Command::Action::factory(gtk_entry_get_text(GTK_ENTRY(activate_entry)), &error);
    if (error) {
        show_error_dialog(GTK_WINDOW(dialog), error->message);
        return false;
    }
    cmd->deactivation_action = Command::Action::factory(gtk_entry_get_text(GTK_ENTRY(deactivate_entry)), &error);
    if (error) {
        show_error_dialog(GTK_WINDOW(dialog), error->message);
        return false;
    }
    g_autofree gchar *trigger_str = cmd->get_trigger_str();
    const char *exec_str = cmd->activation_action ? cmd->activation_action->get_cmd_str() : "";
    const char *resume_exec_str = cmd->deactivation_action ? cmd->deactivation_action->get_cmd_str() : "";
    g_autoptr(GVariant) res = g_dbus_proxy_call_sync(
        proxy,
        "AddAction",
        g_variant_new("(ssss)",
                      cmd->name.c_str(),
                      trigger_str,
                      exec_str,
                      resume_exec_str),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );
    if (error) {
        show_error_dialog(GTK_WINDOW(dialog), error->message);
        return false;
    }
    g_variant_get(res, "(i)", &cmd->id);

    // add new row
    id_to_action_info[cmd->id] = ActionInfo{cmd};
    XidlechainAppWindow *win = xidlechain_app_get_window(app);
    GtkWidget *actions_box = xidlechain_app_window_get_actions_box(win);
    add_actions_row(GTK_LIST_BOX(actions_box), this, cmd);

    return true;
}

void XidlechainAppController::on_setting_changed(GtkWidget *widget, const char *property_name) {
    g_autoptr(GError) error = NULL;
    bool enabled = gtk_switch_get_active(GTK_SWITCH(widget));
    g_autoptr(GVariant) res = g_dbus_proxy_call_sync(
        proxy,
        "org.freedesktop.DBus.Properties.Set",
        g_variant_new("(ssv)",
                      Xidlechain::DBUS_BUS_NAME,
                      property_name,
                      g_variant_new_boolean(enabled)),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );
    if (error) {
        GtkWindow *win = gtk_application_get_active_window(GTK_APPLICATION(app));
        show_error_dialog(win, error->message);
        // TODO: revert old value
    }
}

// action_props must have type "a{sv}"
shared_ptr<Command> XidlechainAppController::get_command(int action_id, GVariant *action_props, string &error_message) {
    g_autoptr(GError) error = NULL;
    shared_ptr<Command> cmd = make_shared<Command>();
    cmd->id = action_id;

    g_autofree gchar *name = NULL;
    if (!g_variant_lookup(action_props, "Name", "s", &name)) {
        error_message = "Could not get name of action";
        return shared_ptr<Command>(nullptr);
    }
    cmd->name = string(name);

    g_autofree gchar *trigger = NULL;
    if (!g_variant_lookup(action_props, "Trigger", "s", &trigger)) {
        error_message = "Could not get trigger of action";
        return shared_ptr<Command>(nullptr);
    }
    if (!cmd->set_trigger_from_str(trigger, &error)) {
        error_message = string(error->message);
        return shared_ptr<Command>(nullptr);
    }

    g_autofree gchar *exec_str = NULL;
    if (!g_variant_lookup(action_props, "Exec", "s", &exec_str)) {
        error_message = "Could not get Exec of action";
        return shared_ptr<Command>(nullptr);
    }
    cmd->activation_action = Command::Action::factory(exec_str, &error);
    if (error) {
        error_message = string(error->message);
        return shared_ptr<Command>(nullptr);
    }

    g_autofree gchar *resume_exec_str = NULL;
    if (!g_variant_lookup(action_props, "ResumeExec", "s", &resume_exec_str)) {
        error_message = "Could not get ResumeExec of action";
        return shared_ptr<Command>(nullptr);
    }
    cmd->deactivation_action = Command::Action::factory(resume_exec_str, &error);
    if (error) {
        error_message = string(error->message);
        return shared_ptr<Command>(nullptr);
    }

    return cmd;
}

bool XidlechainAppController::get_all_actions(string &error_message) {
    g_autoptr(GError) err = NULL;
    g_autofree gchar *object_manager_path = g_strdup_printf("%s/action", Xidlechain::DBUS_OBJECT_BASE_PATH);
    g_autofree gchar *action_path_format = g_strdup_printf("%s/%%d", object_manager_path);
    g_autoptr(GDBusProxy) proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
        NULL,
        Xidlechain::DBUS_BUS_NAME,
        object_manager_path,
        "org.freedesktop.DBus.ObjectManager",
        NULL,
        &err
    );

    g_autoptr(GVariant) res = g_dbus_proxy_call_sync(
        proxy,
        "GetManagedObjects",
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &err
    );
    if (err) {
        error_message = string(err->message);
        return false;
    }
    g_autoptr(GVariantIter) iter = NULL;
    g_variant_get(res, "(a{oa{sa{sv}}})", &iter);
    gchar *action_path = NULL;
    GVariantIter *iface_iter = NULL;
    // The vararg pointers are freed automatically by g_variant_iter_loop.
    // See https://docs.gtk.org/glib/method.VariantIter.loop.html.
    while (g_variant_iter_loop(iter, "{oa{sa{sv}}}", &action_path, &iface_iter)) {
        int action_id;
        if (sscanf(action_path, action_path_format, &action_id) != 1) {
            g_warning("Could not extract action ID from path %s", action_path);
            continue;
        }
        gchar *iface_name = NULL;
        GVariant *action_props = NULL;
        while (g_variant_iter_loop(iface_iter, "{s@a{sv}}", &iface_name, &action_props)) {
            if (g_strcmp0(iface_name, Xidlechain::DBUS_ACTION_IFACE) == 0) {
                shared_ptr<Command> cmd = get_command(action_id, action_props, error_message);
                if (!cmd) {
                    // inner loop...
                    g_free(iface_name);
                    g_variant_unref(action_props);
                    // outer loop...
                    g_free(action_path);
                    g_variant_iter_free(iface_iter);
                    return false;
                }
                id_to_action_info[action_id] = ActionInfo{cmd};
            }
        }
    }

    return true;
}

bool XidlechainAppController::get_all_properties(string &error_message) {
    g_autoptr(GError) err = NULL;
    g_autoptr(GVariant) res = g_dbus_proxy_call_sync(
        proxy,
        "org.freedesktop.DBus.Properties.GetAll",
        g_variant_new("(s)", Xidlechain::DBUS_BUS_NAME),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &err
    );
    if (err) {
        error_message = string(err->message);
        return false;
    }
    g_autoptr(GVariant) arr = NULL;
    // Take the array out of the tuple
    g_variant_get(res, "(@a{sv})", &arr);
    // We don't have that many keys, so a linear scan is good enough
    if (!g_variant_lookup(arr, "Paused", "b", &paused)) {
        error_message = "Could not get Paused";
        return false;
    }
    if (!g_variant_lookup(arr, "IgnoreAudio", "b", &ignore_audio)) {
        error_message = "Could not get IgnoreAudio";
        return false;
    }
    if (!g_variant_lookup(arr, "WaitBeforeSleep", "b", &wait_before_sleep)) {
        error_message = "Could not get WaitBeforeSleep";
        return false;
    }
    if (!g_variant_lookup(arr, "DisableAutomaticDPMSActivation", "b", &disable_automatic_dpms_activation)) {
        error_message = "Could not get DisableAutomaticDPMSActivation";
        return false;
    }
    if (!g_variant_lookup(arr, "DisableScreensaver", "b", &disable_screensaver)) {
        error_message = "Could not get DisableScreensaver";
        return false;
    }
    if (!g_variant_lookup(arr, "WakeResumesActivity", "b", &wake_resumes_activity)) {
        error_message = "Could not get WakeResumesActivity";
        return false;
    }
    if (!g_variant_lookup(arr, "Paused", "b", &paused)) {
        error_message = "Could not get Paused";
        return false;
    }

    return true;
}

bool XidlechainAppController::init(string &error_message) {
    g_autoptr(GError) err = NULL;
    proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
        NULL,
        Xidlechain::DBUS_BUS_NAME,
        Xidlechain::DBUS_OBJECT_BASE_PATH,
        Xidlechain::DBUS_BUS_NAME,
        NULL,
        &err
    );
    if (err) {
        error_message = string(err->message);
        return false;
    }
    INSTANCE = this;
    return get_all_properties(error_message)
        && get_all_actions(error_message);
}

void XidlechainAppController::associate_action_id_with_row(int action_id, GtkWidget *row, GtkWidget *name_label) {
    unordered_map<int, ActionInfo>::iterator it = id_to_action_info.find(action_id);
    g_assert(it != id_to_action_info.end());
    ActionInfo &action_info = it->second;
    action_info.row = row;
    action_info.name_label = name_label;
}

XidlechainAppController::ActionInfoMapValues XidlechainAppController::get_commands() {
    std::function<shared_ptr<Command>(std::pair<const int, ActionInfo>)> f
        = [](std::pair<const int, ActionInfo> p) {
            return p.second.cmd;
        };
    return Map(
        id_to_action_info.begin(),
        id_to_action_info.end(),
        std::move(f)
    );
}

XidlechainAppController::~XidlechainAppController() {
    if (proxy) {
        g_object_unref(proxy);
    }
}
