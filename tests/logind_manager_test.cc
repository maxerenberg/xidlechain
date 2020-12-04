#include <iostream>
#include "event_receiver.h"
#include "logind_manager.h"

using namespace std;

class MyReceiver: public Xidlechain::EventReceiver {
public:
    void receive(Xidlechain::EventType event, gpointer) override {
        switch (event) {
            case Xidlechain::EVENT_LOCK:
                cout << "EVENT_LOCK" << endl;
                break;
            case Xidlechain::EVENT_UNLOCK:
                cout << "EVENT_UNLOCK" << endl;
                break;
            case Xidlechain::EVENT_SLEEP:
                cout << "EVENT_SLEEP" << endl;
                break;
            case Xidlechain::EVENT_WAKE:
                cout << "EVENT_WAKE" << endl;
                break;
            default:
                break;
        }
    }
};

int main() {
    Xidlechain::LogindManager manager;
    MyReceiver receiver;
    if (!manager.init(&receiver)) {
        return 1;
    }
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    return 0;
}
