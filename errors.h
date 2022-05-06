#ifndef _ERRORS_H_
#define _ERRORS_H_

#include <glib.h>

G_BEGIN_DECLS

#define XIDLECHAIN_ERROR xidlechain_error_quark ()
GQuark xidlechain_error_quark(void);

typedef enum {
    XIDLECHAIN_ERROR_INVALID_TRIGGER,
    XIDLECHAIN_ERROR_INVALID_ACTION,
} XidlechainError;

G_END_DECLS

#endif
