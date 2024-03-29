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
        EVENT_AUDIO_RUNNING,
        EVENT_AUDIO_STOPPED,
        EVENT_CONFIG_CHANGED,
        EVENT_COMMAND_CHANGED,
        EVENT_COMMAND_ADDED,
        EVENT_COMMAND_REMOVED,
        EVENT_PAUSED,
        EVENT_UNPAUSED,
    };

    class EventReceiver {
    public:
        virtual void receive(EventType event, gpointer data) = 0;
    protected:
        ~EventReceiver() = default;
    };
}

#endif
