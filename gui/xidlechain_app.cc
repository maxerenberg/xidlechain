#include <gtk/gtk.h>

#include "xidlechain_app.h"
#include "xidlechain_app_controller.h"
#include "xidlechain_app_window.h"

using std::string;

struct _XidlechainApp {
    GtkApplication parent;
    // We can't place a C++ object in this struct or else its constructor
    // won't run
    XidlechainAppController *controller;
    XidlechainAppWindow *win;
};

G_DEFINE_TYPE(XidlechainApp, xidlechain_app, GTK_TYPE_APPLICATION);

static void
xidlechain_app_init (XidlechainApp *app)
{
    app->controller = new XidlechainAppController();
    app->controller->app = app;
    app->win = nullptr;
}

// Adapted from
// https://www.manpagez.com/html/gobject/gobject-2.56.0/howto-gobject-destruction.php
static void
xidlechain_app_finalize (GObject *gobject)
{
    XidlechainApp *app = XIDLECHAIN_APP (gobject);
    delete app->controller;
    app->controller = nullptr;
    if (app->win) {
        g_clear_object(&app->win);
    }
    // Chain up to the parent class
    G_OBJECT_CLASS (xidlechain_app_parent_class)->finalize (gobject);
}

static void
show_startup_error_message (const string &error_message)
{
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new (NULL,
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     "Could not connect to xidlechain over DBus.\n\n%s",
                                     error_message.c_str());
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}

static void
xidlechain_app_activate (GApplication *gapp)
{
    XidlechainApp *app;
    string error_message;

    app = XIDLECHAIN_APP (gapp);
    if (!app->controller->init(error_message)) {
        show_startup_error_message (error_message);
        g_abort();
    }

    app->win = xidlechain_app_window_new (app);
    gtk_window_present (GTK_WINDOW (app->win));
}

static void
xidlechain_app_class_init (XidlechainAppClass *klass)
{
    G_APPLICATION_CLASS (klass)->activate = xidlechain_app_activate;
    G_OBJECT_CLASS (klass)->finalize = xidlechain_app_finalize;
}

XidlechainApp *
xidlechain_app_new (void)
{
    return (XidlechainApp*)
           g_object_new (XIDLECHAIN_APP_TYPE,
                         "application-id", "io.github.maxerenberg.xidlechain.gui",
                         NULL);
}

XidlechainAppController *
xidlechain_app_get_controller (XidlechainApp *app)
{
    return app->controller;
}

XidlechainAppWindow *
xidlechain_app_get_window (XidlechainApp *app)
{
    return app->win;
}
