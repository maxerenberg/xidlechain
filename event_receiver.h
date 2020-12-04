#ifndef _EVENT_RECEVER_H_
#define _EVENT_RECEVER_H_

#include <glib.h>

namespace Xidlechain {
    enum EventType {
        EVENT_ACTIVITY_TIMEOUT = 1,
        EVENT_ACTIVITY_RESUME,
        EVENT_SLEEP,
        EVENT_WAKE,
        EVENT_LOCK,
        EVENT_UNLOCK,
    };

    class EventReceiver {
    public:
        virtual void receive(EventType event, gpointer data) = 0;
    protected:
        ~EventReceiver() = default;
    };
}

#endif