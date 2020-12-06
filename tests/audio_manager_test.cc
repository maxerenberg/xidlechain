#include <iostream>
#include <glib.h>
#include "audio_manager.h"

using namespace std;

class MyReceiver: public Xidlechain::EventReceiver {
public:
    void receive(Xidlechain::EventType event, gpointer data) override {
        switch (event) {
            case Xidlechain::EVENT_AUDIO_RUNNING:
                cout << "AUDIO RUNNING" << endl;
                break;
            case Xidlechain::EVENT_AUDIO_STOPPED:
                cout << "AUDIO STOPPED" << endl;
                break;
            default:
                break;
        }
    }
};

int main() {
    MyReceiver receiver;
    Xidlechain::AudioManager audio_manager;
    if (!audio_manager.init(&receiver)) {
        return 1;
    }

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    return 0;
}