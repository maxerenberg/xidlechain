#include <iostream>
#include <gdk/gdk.h>
#include "activity_detector.h"
#include "event_receiver.h"

using namespace std;

class MyReceiver: public Xidlechain::EventReceiver {
public:
    void receive(Xidlechain::EventType event, gpointer data) override {
        switch (event) {
            case Xidlechain::EVENT_ACTIVITY_TIMEOUT:
                cout << "TIMEOUT " << (long)data << endl;
                break;
            case Xidlechain::EVENT_ACTIVITY_RESUME:
                cout << "RESUME" << endl;
                break;
            default:
                break;
        }
    }
};

int main(int argc, char *argv[]) {
    gdk_init(&argc, &argv);
    MyReceiver receiver;
    Xidlechain::XsyncActivityDetector activity_detector;
    activity_detector.init(&receiver);
    activity_detector.add_idle_timeout(2000, (gpointer)1L);
    activity_detector.add_idle_timeout(5000, (gpointer)2L);
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
}
