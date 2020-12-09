#include <glib.h>
#include <locale>
#include <vector>
#include "activity_detector.h"
#include "audio_detector.h"
#include "event_manager.h"
#include "logind_manager.h"
#include "process_spawner.h"

using std::vector;
using namespace Xidlechain;

class MockActivityDetector: public ActivityDetector {
public:
    vector<gpointer> cb_data;
    bool init(EventReceiver *receiver) override {
        return true;
    }
    bool add_idle_timeout(int64_t timeout_ms, gpointer data) override {
        cb_data.push_back(data);
        return true;
    }
    bool clear_timeouts() override {
        cb_data.clear();
        return true;
    }
    void reset() {
        cb_data.clear();
    }
};

class MockAudioDetector: public AudioDetector {
public:
    bool initialized = false;
    bool init(EventReceiver *receiver) override {
        initialized = true;
        return true;
    }
    void reset() {
        initialized = false;
    }
};

class MockLogindManager: public LogindManager {
public:
    vector<bool> idle_hint_history;
    bool init(EventReceiver *receiver) override {
        return true;
    }
    bool set_idle_hint(bool idle) override {
        idle_hint_history.push_back(idle);
        return true;
    }
    void reset() {
        idle_hint_history.clear();
    }
};

class MockProcessSpawner: public ProcessSpawner {
public:
    int kill_children_call_count = 0;
    vector<char*> sync_cmds,
                  async_cmds;
    void exec_cmd_sync(char *cmd) override {
        sync_cmds.push_back(cmd);
    }
    void exec_cmd_async(char *cmd) override {
        async_cmds.push_back(cmd);
    }
    void kill_children() override {
        kill_children_call_count++;
        sync_cmds.clear();
        async_cmds.clear();
    }
    void reset() {
        kill_children_call_count = 0;
        sync_cmds.clear();
        async_cmds.clear();
    }
};

MockActivityDetector activity_detector;
MockAudioDetector audio_detector;
MockLogindManager logind_manager;
MockProcessSpawner process_spawner;

static void fixture_setup(gpointer fixture, gconstpointer user_data) {
    activity_detector.reset();
    audio_detector.reset();
    logind_manager.reset();
    process_spawner.reset();
}

static bool event_manager_init(EventManager &event_manager) {
    return event_manager.init(&activity_detector,
                              &logind_manager,
                              &audio_detector,
                              &process_spawner);
}

static void test_activity_1(gpointer, gconstpointer user_data) {
    bool wait_before_sleep = false,
         kill_on_resume = false,
         ignore_audio = false;
    int num_timeouts = (long)user_data;
    EventManager event_manager(
        wait_before_sleep, kill_on_resume, ignore_audio);
    event_manager_init(event_manager);
    char before_cmds[][3] = {"b1", "b2"};
    char after_cmds[][3] = {"a1", "a2"};
    // It shouldn't matter if the timeouts are passed in non-sorted order
    int64_t timeouts[] = {3000, 2000};
    // add both timeouts
    event_manager.add_timeout_command(before_cmds[0], after_cmds[0], timeouts[0]);
    event_manager.add_timeout_command(before_cmds[1], after_cmds[1], timeouts[1]);
    g_assert_cmpuint(activity_detector.cb_data.size(), ==, 2);
    // trigger each timeout one at a time
    event_manager.receive(EVENT_ACTIVITY_TIMEOUT, activity_detector.cb_data[1]);
    g_assert_cmpuint(process_spawner.async_cmds.size(), ==, 1);
    if (num_timeouts == 2) {
        event_manager.receive(EVENT_ACTIVITY_TIMEOUT, activity_detector.cb_data[0]);
        g_assert_cmpuint(process_spawner.async_cmds.size(), ==, 2);
    }
    // verify that the timeouts were executed in order of increasing timeout
    g_assert_cmpstr(process_spawner.async_cmds[0], ==, before_cmds[1]);
    if (num_timeouts == 2) {
        g_assert_cmpstr(process_spawner.async_cmds[1], ==, before_cmds[0]);
    }
    // now resume activity
    event_manager.receive(EVENT_ACTIVITY_RESUME, NULL);
    // kill_children() should not have been called
    g_assert_cmpuint(process_spawner.kill_children_call_count, ==, 0);
    // verify that the "after" commands were run only for the timeouts which
    // went off
    g_assert_cmpuint(process_spawner.async_cmds.size(), ==, num_timeouts * 2);
}

static void test_sleep_1(gpointer, gconstpointer user_data) {
    bool kill_on_resume = false, ignore_audio = false;
    bool wait_before_sleep = (bool)user_data;
    EventManager event_manager(
        wait_before_sleep, kill_on_resume, ignore_audio);
    event_manager_init(event_manager);
    event_manager.set_sleep_cmd("s1");
    event_manager.set_wake_cmd("r1");
    // send a SLEEP signal
    event_manager.receive(EVENT_SLEEP, NULL);
    if (wait_before_sleep) {
        g_assert_cmpuint(process_spawner.sync_cmds.size(), ==, 1);
    } else {
        g_assert_cmpuint(process_spawner.async_cmds.size(), ==, 1);
    }
    // send a WAKE signal
    event_manager.receive(EVENT_WAKE, NULL);
    // clear_children() should _not_ have been called since an
    // ACTIVITY_RESUME signal was not sent
    if (wait_before_sleep) {
        g_assert_cmpuint(process_spawner.async_cmds.size(), ==, 1);
    } else {
        g_assert_cmpuint(process_spawner.async_cmds.size(), ==, 2);
    }
}

static void test_sleep_2(gpointer, gconstpointer user_data) {
    bool wait_before_sleep = false, ignore_audio = false;
    bool kill_on_resume = (bool)user_data;
    EventManager event_manager(
        wait_before_sleep, kill_on_resume, ignore_audio);
    event_manager_init(event_manager);
    event_manager.add_timeout_command("b1", "a1", 2000);
    // send a TIMEOUT, then a RESUME
    event_manager.receive(EVENT_ACTIVITY_TIMEOUT, activity_detector.cb_data.at(0));
    event_manager.receive(EVENT_ACTIVITY_RESUME, NULL);
    g_assert_cmpint(process_spawner.kill_children_call_count,
                    ==, (int)kill_on_resume);
}

static void test_audio_1(gpointer, gconstpointer user_data) {
    bool kill_on_resume = false, wait_before_sleep = false;
    bool ignore_audio = (bool)user_data;
    EventManager event_manager(
        wait_before_sleep, kill_on_resume, ignore_audio);
    event_manager_init(event_manager);
    if (ignore_audio) {
        g_assert_cmpint(audio_detector.initialized, ==, 0);
        return;
    }
    event_manager.add_timeout_command("b1", "a1", 2000);

    event_manager.receive(EVENT_AUDIO_RUNNING, NULL);
    // timeout should have been cleared
    g_assert_cmpuint(activity_detector.cb_data.size(), ==, 0);
    event_manager.receive(EVENT_AUDIO_STOPPED, NULL);
    // timeout should have been restored
    g_assert_cmpuint(activity_detector.cb_data.size(), ==, 1);
    event_manager.receive(EVENT_ACTIVITY_TIMEOUT, activity_detector.cb_data[0]);
    // "before" command should have been executed
    g_assert_cmpuint(process_spawner.async_cmds.size(), ==, 1);
}

static void test_idlehint(gpointer, gconstpointer) {
    EventManager event_manager(false, false, false);
    event_manager_init(event_manager);
    event_manager.enable_idle_hint(2000);
    // verify that a timeout was registered
    g_assert_cmpuint(activity_detector.cb_data.size(), ==, 1);
    // IdleHint is set to false when we first start
    g_assert_cmpint(logind_manager.idle_hint_history.at(0), ==, false);
    event_manager.receive(EVENT_ACTIVITY_TIMEOUT, activity_detector.cb_data[0]);
    // IdleHint should be set to true
    g_assert_cmpint(logind_manager.idle_hint_history.at(1), ==, true);
    event_manager.receive(EVENT_ACTIVITY_RESUME, activity_detector.cb_data[0]);
    // IdleHint should be set to false
    g_assert_cmpint(logind_manager.idle_hint_history.at(2), ==, false);
}

static void test_lock(gpointer, gconstpointer) {
    EventManager event_manager(false, false, false);
    event_manager_init(event_manager);
    event_manager.set_lock_cmd("l1");
    event_manager.set_unlock_cmd("u1");

    event_manager.receive(EVENT_LOCK, NULL);
    g_assert_cmpstr(process_spawner.async_cmds.at(0), ==, "l1");
    // Even if a LOCK event is sent twice without an UNLOCK event in
    // between, the command should still be exected
    event_manager.receive(EVENT_LOCK, NULL);
    g_assert_cmpstr(process_spawner.async_cmds.at(1), ==, "l1");
    event_manager.receive(EVENT_UNLOCK, NULL);
    g_assert_cmpstr(process_spawner.async_cmds.at(2), ==, "u1");
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    g_test_add("/event-manager/two-cmds-one-timeout", void, (gconstpointer)1,
               fixture_setup, test_activity_1, NULL);
    g_test_add("/event-manager/two-cmds-two-timeouts", void, (gconstpointer)2,
               fixture_setup, test_activity_1, NULL);
    g_test_add("/event-manager/wait-before-sleep", void, (gconstpointer)1,
               fixture_setup, test_sleep_1, NULL);
    g_test_add("/event-manager/no-wait-before-sleep", void, (gconstpointer)0,
               fixture_setup, test_sleep_1, NULL);
    g_test_add("/event-manager/kill-on-resume", void, (gconstpointer)1,
               fixture_setup, test_sleep_2, NULL);
    g_test_add("/event-manager/no-kill-on-resume", void, (gconstpointer)0,
               fixture_setup, test_sleep_2, NULL);
    g_test_add("/event-manager/ignore-audio", void, (gconstpointer)1,
               fixture_setup, test_audio_1, NULL);
    g_test_add("/event-manager/no-ignore-audio", void, (gconstpointer)0,
               fixture_setup, test_audio_1, NULL);
    g_test_add("/event-manager/idlehint", void, NULL,
               fixture_setup, test_idlehint, NULL);
    g_test_add("/event-manager/lock-unlock", void, NULL,
               fixture_setup, test_lock, NULL);

    return g_test_run();
}
