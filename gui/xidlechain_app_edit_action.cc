#include "xidlechain_app_edit_action.h"

struct _XidlechainAppEditAction
{
    GtkDialog parent;
};

typedef struct _XidlechainAppEditActionPrivate XidlechainAppEditActionPrivate;

struct _XidlechainAppEditActionPrivate
{
    GtkWidget *name_entry;
    GtkWidget *trigger_combobox;
    GtkWidget *seconds_entry;
    GtkWidget *activate_entry;
    GtkWidget *deactivate_entry;
};

G_DEFINE_TYPE_WITH_PRIVATE(XidlechainAppEditAction, xidlechain_app_edit_action, GTK_TYPE_DIALOG)

static void
xidlechain_app_edit_action_init (XidlechainAppEditAction *dialog)
{
    gtk_widget_init_template (GTK_WIDGET (dialog));
}

static void
xidlechain_app_edit_action_class_init (XidlechainAppEditActionClass *klass)
{
    gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                                 "/io/github/maxerenberg/xidlechain/gui/edit_action.ui");
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), XidlechainAppEditAction, name_entry);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), XidlechainAppEditAction, trigger_combobox);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), XidlechainAppEditAction, seconds_entry);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), XidlechainAppEditAction, activate_entry);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), XidlechainAppEditAction, deactivate_entry);
}

XidlechainAppEditAction *
xidlechain_app_edit_action_new (XidlechainAppWindow *win)
{
    XidlechainAppEditAction *dialog;
    dialog = (XidlechainAppEditAction*)
             g_object_new (XIDLECHAIN_APP_EDIT_ACTION_TYPE,
                           "transient-for", win,
                           "use-header-bar", TRUE,
                           NULL);
    return dialog;
}

static XidlechainAppEditActionPrivate *
get_private (XidlechainAppEditAction *dialog)
{
    return (XidlechainAppEditActionPrivate*) xidlechain_app_edit_action_get_instance_private (dialog);
}

GtkWidget *
xidlechain_app_edit_action_get_name_entry (XidlechainAppEditAction *dialog)
{
    return get_private (dialog)->name_entry;
}

GtkWidget *
xidlechain_app_edit_action_get_trigger_combobox (XidlechainAppEditAction *dialog)
{
    return get_private (dialog)->trigger_combobox;
}

GtkWidget *
xidlechain_app_edit_action_get_seconds_entry (XidlechainAppEditAction *dialog)
{
    return get_private (dialog)->seconds_entry;
}

GtkWidget *
xidlechain_app_edit_action_get_activate_entry (XidlechainAppEditAction *dialog)
{
    return get_private (dialog)->activate_entry;
}

GtkWidget *
xidlechain_app_edit_action_get_deactivate_entry (XidlechainAppEditAction *dialog)
{
    return get_private (dialog)->deactivate_entry;
}
