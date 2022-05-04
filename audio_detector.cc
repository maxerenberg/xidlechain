#include "app.h"
#include "audio_detector.h"
#include "event_receiver.h"

using std::pair;
using std::unordered_set;

namespace Xidlechain {
    PulseAudioDetector::PulseAudioDetector():
        event_receiver(NULL),
        loop(NULL),
        api(NULL),
        ctx(NULL),
        sent_first_message(false)
    {}

    PulseAudioDetector::~PulseAudioDetector() {
        if (ctx) {
            pa_context_unref(ctx);
        }
        if (loop) {
            pa_glib_mainloop_free(loop);
        }
    }

    bool PulseAudioDetector::init(EventReceiver *receiver) {
        int status;
        g_return_val_if_fail(receiver != NULL, FALSE);
        event_receiver = receiver;

        loop = pa_glib_mainloop_new(NULL);
        g_return_val_if_fail(loop != NULL, FALSE);

        api = pa_glib_mainloop_get_api(loop);
        ctx = pa_context_new(api, APP_NAME);
        g_return_val_if_fail(ctx != NULL, FALSE);

        status = pa_context_connect(ctx, NULL, PA_CONTEXT_NOFLAGS, NULL);
        g_return_val_if_fail(status >= 0, FALSE);

        pa_context_set_state_callback(ctx, context_notify_cb, this);
        return true;
    }

    void PulseAudioDetector::add_sink(int idx) {
        bool inserted = running_sinks.insert(idx).second;
        if ((running_sinks.size() == 1 && inserted) || !sent_first_message) {
            event_receiver->receive(EVENT_AUDIO_RUNNING, NULL);
            sent_first_message = true;
        }
    }

    void PulseAudioDetector::remove_sink(int idx) {
        bool erased = running_sinks.erase(idx) > 0;
        if ((running_sinks.size() == 0 && erased) || !sent_first_message) {
            event_receiver->receive(EVENT_AUDIO_STOPPED, NULL);
            sent_first_message = true;
        }
    }

    void PulseAudioDetector::context_notify_cb(pa_context *ctx, void *userdata) {
        switch (pa_context_get_state(ctx)) {
            case PA_CONTEXT_READY: {
                pa_context_get_sink_info_list(ctx, sink_info_cb, userdata);

                pa_context_set_subscribe_callback(ctx, context_subscribe_cb, userdata);
                pa_operation *op = pa_context_subscribe(
                    ctx,
                    PA_SUBSCRIPTION_MASK_SINK,
                    context_success_cb,
                    userdata);
                g_return_if_fail(op != NULL);
                pa_operation_unref(op);
                break;
            }
            case PA_CONTEXT_FAILED:
                g_warning("pulseaudio connection failed or was disconnected");
                break;
            default:
                break;
        }
    }

    void PulseAudioDetector::sink_info_cb(pa_context *ctx, const pa_sink_info *info,
                                    int eol, void *userdata)
    {
        if (eol > 0) {  // end of list was reached
            return;
        } else if (eol < 0) {
            g_warning("Error occurred querying pulseaudio server");
            return;
        }
        PulseAudioDetector *_this = static_cast<PulseAudioDetector*>(userdata);
        switch (info->state) {
            case PA_SINK_RUNNING:
                _this->add_sink(info->index);
                break;
            default: {
                _this->remove_sink(info->index);
                break;
            }
        }
    }

    void PulseAudioDetector::context_success_cb(pa_context *ctx, int success,
                                          void *userdata)
    {
        if (!success) {
            g_critical("Failed to subscribe to pulseaudio events");
        }
    }

    void PulseAudioDetector::context_subscribe_cb(
            pa_context *ctx,
            pa_subscription_event_type_t event_type,
            uint32_t idx,
            void *userdata)
    {
        PulseAudioDetector *_this = static_cast<PulseAudioDetector*>(userdata);
        switch (event_type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
            case PA_SUBSCRIPTION_EVENT_SINK: {
                if ((event_type & PA_SUBSCRIPTION_EVENT_TYPE_MASK)
                    == PA_SUBSCRIPTION_EVENT_REMOVE)
                {
                    g_info("Sink %d was removed", idx);
                    _this->remove_sink(idx);
                } else {
                    pa_operation *op = pa_context_get_sink_info_by_index(
                        ctx, idx, sink_info_cb, userdata);
                    g_return_if_fail(op != NULL);
                    pa_operation_unref(op);
                }
                break;
            }
        }
    }
}
