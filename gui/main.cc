#include <gtk/gtk.h>

#include "xidlechain_app.h"

int
main (int argc, char *argv[])
{
    g_autoptr(XidlechainApp) app = xidlechain_app_new ();
    return g_application_run (G_APPLICATION (app), argc, argv);
}

