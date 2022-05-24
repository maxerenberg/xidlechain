#ifndef _XIDLECHAIN_APP_H_
#define _XIDLECHAIN_APP_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XIDLECHAIN_APP_TYPE (xidlechain_app_get_type ())
G_DECLARE_FINAL_TYPE (XidlechainApp, xidlechain_app, XIDLECHAIN, APP, GtkApplication)

XidlechainApp *xidlechain_app_new (void);

struct XidlechainAppController;
struct XidlechainAppController *xidlechain_app_get_controller (XidlechainApp *app);
struct _XidlechainAppWindow;
struct _XidlechainAppWindow *xidlechain_app_get_window (XidlechainApp *app);

G_END_DECLS

#endif
