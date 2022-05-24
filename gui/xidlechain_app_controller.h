#ifndef _XIDLECHAIN_APP_CONTROLLER_H_
#define _XIDLECHAIN_APP_CONTROLLER_H_

#include <memory>
#include <string>

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "config_manager.h"
#include "map.h"

struct _XidlechainApp;
struct _XidlechainAppEditAction;

struct ActionInfo {
    shared_ptr<Xidlechain::Command> cmd;
    GtkWidget *row = nullptr;
    GtkWidget *name_label = nullptr;
};

class XidlechainAppController {
    static XidlechainAppController *INSTANCE;
    GDBusProxy *proxy = nullptr;
    std::unordered_map<int, ActionInfo> id_to_action_info;

    std::shared_ptr<Xidlechain::Command> get_command(int action_id, GVariant *action_props, std::string &error_message);
    bool get_all_properties(std::string &error_message);
    bool get_all_actions(std::string &error_message);
    void on_pause_changed(GtkWidget *widget);
    void on_setting_changed(GtkWidget *widget, const char *property_name);
    void on_add_action_clicked();
    bool on_action_added(_XidlechainAppEditAction *dialog);
    void on_action_edit_clicked(gpointer user_data);
    void on_action_delete_clicked(gpointer user_data);
    bool on_action_edited(_XidlechainAppEditAction *dialog, std::shared_ptr<Xidlechain::Command> cmd);
    bool set_action_property(
        GDBusProxy *action_proxy,
        const char *property_name,
        const char *value,
        std::string &error_message
    );
public:
    using ActionInfoMapValues =
        Map<
            std::unordered_map<int, ActionInfo>::iterator,
            std::shared_ptr<Xidlechain::Command>
        >;

    _XidlechainApp *app = nullptr;
    bool ignore_audio;
    bool wait_before_sleep;
    bool disable_automatic_dpms_activation;
    bool disable_screensaver;
    bool wake_resumes_activity;
    bool paused;

    static void static_on_pause_changed(GtkWidget *widget, gpointer data);
    template<const char *property_name>
    static void static_on_setting_changed(GtkWidget *widget, gpointer data);
    static void static_on_add_action_clicked(GtkButton *button, gpointer user_data);
    static void static_on_action_edit_clicked(GtkButton *button, gpointer user_data);
    static void static_on_action_delete_clicked(GtkButton *button, gpointer user_data);

    bool init(std::string &error_message);
    ActionInfoMapValues get_commands();
    void associate_action_id_with_row(int action_id, GtkWidget *row, GtkWidget *name_label);
    ~XidlechainAppController();
};

template<const char *property_name>
void XidlechainAppController::static_on_setting_changed(GtkWidget *widget, gpointer) {
    INSTANCE->on_setting_changed(widget, property_name);
}

#endif
