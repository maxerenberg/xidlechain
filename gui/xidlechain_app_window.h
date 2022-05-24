#ifndef _XIDLECHAIN_APP_WINDOW_H_
#define _XIDLECHAIN_APP_WINDOW_H_

#include <memory>
#include <gtk/gtk.h>
#include "command.h"
#include "xidlechain_app.h"

G_BEGIN_DECLS

#define XIDLECHAIN_APP_WINDOW_TYPE (xidlechain_app_window_get_type ())
G_DECLARE_FINAL_TYPE (XidlechainAppWindow, xidlechain_app_window, XIDLECHAIN, APP_WINDOW, GtkApplicationWindow)

XidlechainAppWindow *xidlechain_app_window_new (XidlechainApp *app);

GtkWidget *xidlechain_app_window_get_actions_box (XidlechainAppWindow *win);

G_END_DECLS

struct XidlechainAppController;
void add_actions_row (GtkListBox                           *actions_box,
                      XidlechainAppController              *controller,
                      std::shared_ptr<Xidlechain::Command> cmd);

#endif
