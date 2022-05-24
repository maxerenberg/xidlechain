#include <memory>
#include <utility>

#include <gtk/gtk.h>

#include "command.h"
#include "xidlechain_app.h"
#include "xidlechain_app_controller.h"
#include "xidlechain_app_window.h"

using std::shared_ptr;
using Xidlechain::Command;

struct _XidlechainAppWindow {
    GtkApplicationWindow parent;
};

typedef struct _XidlechainAppWindowPrivate XidlechainAppWindowPrivate;

struct _XidlechainAppWindowPrivate {
    GtkCssProvider *css_provider;
    GtkStyleContext *style_context;
    GtkWidget *stack;
    GtkWidget *pause_switch;
    GtkWidget *ignore_audio_switch;
    GtkWidget *wait_before_sleep_switch;
    GtkWidget *disable_dpms_switch;
    GtkWidget *disable_screensaver_switch;
    GtkWidget *wake_resumes_activity_switch;
    GtkWidget *actions_box;
    GtkWidget *add_action_button;
};

G_DEFINE_TYPE_WITH_PRIVATE(XidlechainAppWindow, xidlechain_app_window, GTK_TYPE_APPLICATION_WINDOW);

static void
xidlechain_app_window_init (XidlechainAppWindow *win)
{
    gtk_widget_init_template (GTK_WIDGET (win));
    XidlechainAppWindowPrivate *priv;
    priv = (XidlechainAppWindowPrivate*) xidlechain_app_window_get_instance_private (win);
    priv->css_provider = gtk_css_provider_new ();
    priv->style_context = gtk_style_context_new ();
}

static void
xidlechain_app_window_finalize (GObject *gobject)
{
    XidlechainAppWindow *win = XIDLECHAIN_APP_WINDOW (gobject);
    XidlechainAppWindowPrivate *priv;
    priv = (XidlechainAppWindowPrivate*) xidlechain_app_window_get_instance_private (win);
    g_clear_object (&priv->css_provider);
    g_clear_object (&priv->style_context);
    // Chain up to the parent class
    G_OBJECT_CLASS (xidlechain_app_window_parent_class)->finalize (gobject);
}

static void
xidlechain_app_window_class_init (XidlechainAppWindowClass *klass)
{
    gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/io/github/maxerenberg/xidlechain/gui/window.ui");
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), XidlechainAppWindow, stack);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), XidlechainAppWindow, pause_switch);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), XidlechainAppWindow, ignore_audio_switch);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), XidlechainAppWindow, wait_before_sleep_switch);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), XidlechainAppWindow, disable_dpms_switch);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), XidlechainAppWindow, disable_screensaver_switch);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), XidlechainAppWindow, wake_resumes_activity_switch);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), XidlechainAppWindow, actions_box);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), XidlechainAppWindow, add_action_button);
    G_OBJECT_CLASS (klass)->finalize = xidlechain_app_window_finalize;
}

void
add_actions_row (GtkListBox              *actions_box,
                 XidlechainAppController *controller,
                 shared_ptr<Command>     cmd)
{
    GtkWidget *row_box;
    GtkWidget *action_name_text;
    GtkWidget *edit_button;
    GtkStyleContext *edit_button_style;
    GtkWidget *delete_button;
    GtkStyleContext *delete_button_style;
    const int row_horiz_padding = 24;
    const int row_vert_padding = 12;

    row_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_show (row_box);

    action_name_text = gtk_label_new (cmd->name.c_str());
    gtk_widget_show (action_name_text);
    gtk_widget_set_margin_top (action_name_text, row_vert_padding);
    gtk_widget_set_margin_bottom (action_name_text, row_vert_padding);
    gtk_box_pack_start (GTK_BOX (row_box), action_name_text, FALSE, FALSE, row_horiz_padding);
    controller->associate_action_id_with_row(cmd->id, row_box, action_name_text);

    delete_button = gtk_button_new_with_label ("Delete");
    gtk_widget_show (delete_button);
    g_signal_connect (G_OBJECT (delete_button),
                      "clicked",
                      G_CALLBACK (XidlechainAppController::static_on_action_delete_clicked),
                      reinterpret_cast<gpointer>(cmd->id));
    delete_button_style = gtk_widget_get_style_context (delete_button);
    gtk_style_context_add_class (delete_button_style, "destructive-action");
    gtk_widget_set_margin_top (delete_button, row_vert_padding);
    gtk_widget_set_margin_bottom (delete_button, row_vert_padding);
    gtk_box_pack_end (GTK_BOX (row_box), delete_button, FALSE, FALSE, row_horiz_padding);

    edit_button = gtk_button_new_with_label ("Edit");
    gtk_widget_show (edit_button);
    g_signal_connect (G_OBJECT (edit_button),
                      "clicked",
                      G_CALLBACK (XidlechainAppController::static_on_action_edit_clicked),
                      reinterpret_cast<gpointer>(cmd->id));
    edit_button_style = gtk_widget_get_style_context (edit_button);
    gtk_style_context_add_class (edit_button_style, "suggested-action");
    gtk_widget_set_margin_top (edit_button, row_vert_padding);
    gtk_widget_set_margin_bottom (edit_button, row_vert_padding);
    gtk_box_pack_end (GTK_BOX (row_box), edit_button, FALSE, FALSE, 0);

    gtk_container_add (GTK_CONTAINER (actions_box), row_box);
}

static void
prepare_setting_toggle (GtkWidget *toggle,
                        bool      enabled,
                        GCallback callback)
{
    gtk_switch_set_active (GTK_SWITCH (toggle), enabled);
    g_signal_connect (G_OBJECT (toggle), "notify::active", callback, NULL);
}

// const char* can't be passed as a template parameter, but const char[] can.
// See https://stackoverflow.com/a/47661368.
static const char IgnoreAudio[] = "IgnoreAudio";
static const char WaitBeforeSleep[] = "WaitBeforeSleep";
static const char DisableAutomaticDPMSActivation[] = "DisableAutomaticDPMSActivation";
static const char DisableScreensaver[] = "DisableScreensaver";
static const char WakeResumesActivity[] = "WakeResumesActivity";

XidlechainAppWindow *
xidlechain_app_window_new (XidlechainApp *app)
{
    XidlechainAppWindow *win;
    XidlechainAppWindowPrivate *priv;
    XidlechainAppController *controller;
    GtkWidget *actions_box;

    win = (XidlechainAppWindow*) g_object_new (XIDLECHAIN_APP_WINDOW_TYPE, "application", app, NULL);
    priv = (XidlechainAppWindowPrivate*) xidlechain_app_window_get_instance_private (win);
    controller = xidlechain_app_get_controller (app);

    gtk_css_provider_load_from_resource (priv->css_provider,
                                         "/io/github/maxerenberg/xidlechain/gui/styles.css");
    gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                               GTK_STYLE_PROVIDER (priv->css_provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    prepare_setting_toggle (priv->pause_switch,
                            controller->paused,
                            G_CALLBACK (XidlechainAppController::static_on_pause_changed));
    prepare_setting_toggle (priv->ignore_audio_switch,
                            controller->ignore_audio,
                            G_CALLBACK (XidlechainAppController::static_on_setting_changed<IgnoreAudio>));
    prepare_setting_toggle (priv->wait_before_sleep_switch,
                            controller->wait_before_sleep,
                            G_CALLBACK (XidlechainAppController::static_on_setting_changed<WaitBeforeSleep>));
    prepare_setting_toggle (priv->disable_dpms_switch,
                            controller->disable_automatic_dpms_activation,
                            G_CALLBACK (XidlechainAppController::static_on_setting_changed<DisableAutomaticDPMSActivation>));
    prepare_setting_toggle (priv->disable_screensaver_switch,
                            controller->disable_screensaver,
                            G_CALLBACK (XidlechainAppController::static_on_setting_changed<DisableScreensaver>));
    prepare_setting_toggle (priv->wake_resumes_activity_switch,
                            controller->wake_resumes_activity,
                            G_CALLBACK (XidlechainAppController::static_on_setting_changed<WakeResumesActivity>));

    g_signal_connect (G_OBJECT (priv->add_action_button),
                      "clicked",
                      G_CALLBACK (XidlechainAppController::static_on_add_action_clicked),
                      NULL);
    actions_box = priv->actions_box;
    for (shared_ptr<Command> cmd : controller->get_commands()) {
        add_actions_row (GTK_LIST_BOX (actions_box), controller, cmd);
    }

    return win;
}

GtkWidget *
xidlechain_app_window_get_actions_box (XidlechainAppWindow *win)
{
    XidlechainAppWindowPrivate *priv;
    priv = (XidlechainAppWindowPrivate*) xidlechain_app_window_get_instance_private (win);
    return priv->actions_box;
}
