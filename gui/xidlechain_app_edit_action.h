#ifndef _XIDLECHAIN_APP_EDIT_ACTION_H_
#define _XIDLECHAIN_APP_EDIT_ACTION_H_

#include <gtk/gtk.h>
#include "xidlechain_app_window.h"

G_BEGIN_DECLS

#define XIDLECHAIN_APP_EDIT_ACTION_TYPE (xidlechain_app_edit_action_get_type ())
G_DECLARE_FINAL_TYPE (XidlechainAppEditAction, xidlechain_app_edit_action, XIDLECHAIN, APP_EDIT_ACTION, GtkDialog)

XidlechainAppEditAction *xidlechain_app_edit_action_new (XidlechainAppWindow *win);

GtkWidget *xidlechain_app_edit_action_get_name_entry (XidlechainAppEditAction *dialog);
GtkWidget *xidlechain_app_edit_action_get_trigger_combobox (XidlechainAppEditAction *dialog);
GtkWidget *xidlechain_app_edit_action_get_seconds_entry (XidlechainAppEditAction *dialog);
GtkWidget *xidlechain_app_edit_action_get_activate_entry (XidlechainAppEditAction *dialog);
GtkWidget *xidlechain_app_edit_action_get_deactivate_entry (XidlechainAppEditAction *dialog);

G_END_DECLS

#endif
