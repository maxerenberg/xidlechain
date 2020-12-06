#ifndef _AUDIO_MANAGER_H_
#define _AUDIO_MANAGER_H_

#include <unordered_set>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>
#include "event_receiver.h"

using std::unordered_set;

namespace Xidlechain {
    class AudioManager {
        EventReceiver *event_receiver;
        pa_glib_mainloop *loop;
        pa_mainloop_api *api;
        pa_context *ctx;
        // An AUDIO_RUNNING event is sent if there is at least one sink
        // running. If the number of running sinks reaches 0, an AUDIO_STOPPED
        // event is sent. An initial event is always sent after calling init().
        unordered_set<uint32_t> running_sinks;
        bool sent_first_message;

        void add_sink(int idx);
        void remove_sink(int idx);
        static void context_notify_cb(pa_context *ctx, void *userdata);
        static void sink_info_cb(pa_context *ctx, const pa_sink_info *info,
                                 int eol, void *userdata);
        static void context_success_cb(pa_context *ctx, int success,
                                       void *userdata);
        static void context_subscribe_cb(
            pa_context *ctx,
            pa_subscription_event_type_t event_type,
            uint32_t idx,
            void *userdata);
    public:
        AudioManager();
        ~AudioManager();
        bool init(EventReceiver *receiver);
    };
}

#endif