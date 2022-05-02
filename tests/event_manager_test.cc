#include <algorithm>
#include <iostream>
#include <locale>
#include <string>
#include <vector>
#include <glib.h>
#include "activity_detector.h"
#include "audio_detector.h"
#include "brightness_controller.h"
#include "config_manager.h"
#include "event_manager.h"
#include "logind_manager.h"
#include "process_spawner.h"

using std::string;
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
    bool remove_idle_timeout(gpointer data) override {
        vector<gpointer>::iterator it = std::find(cb_data.begin(), cb_data.end(), data);
        if (it != cb_data.end()) {
            cb_data.erase(it);
            return true;
        }
        return false;
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
    bool set_brightness(const char *subsystem, unsigned int value) override {
        return true;
    }
    bool suspend() override {
        return true;
    }
};

class MockProcessSpawner: public ProcessSpawner {
public:
    vector<const char*> sync_cmds,
                        async_cmds;
    void exec_cmd_sync(const string &cmd) override {
        sync_cmds.push_back(cmd.c_str());
    }
    void exec_cmd_async(const string &cmd) override {
        async_cmds.push_back(cmd.c_str());
    }
    void reset() {
        sync_cmds.clear();
        async_cmds.clear();
    }
};

class MockBrightnessController: public BrightnessController {
    bool init(LogindManager *logind_manager) { return true; }
    bool dim() { return true; }
    void restore_brightness() {}
};

MockActivityDetector activity_detector;
MockAudioDetector audio_detector;
MockLogindManager logind_manager;
MockProcessSpawner process_spawner;
MockBrightnessController brightness_controller;

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
                              &process_spawner,
                              &brightness_controller);
}

static Command make_command(
    const char *exec,
    const char *resume_exec,
    int64_t timeout_ms,
    Command::Trigger trigger = Command::TIMEOUT
) {
    Command cmd;
    cmd.trigger = trigger;
    cmd.activation_action = Command::Action::factory(exec);
    cmd.deactivation_action = Command::Action::factory(resume_exec);
    cmd.timeout_ms = timeout_ms;
    return cmd;
}

static void test_activity_1(gpointer, gconstpointer user_data) {
    int num_timeouts = (long)user_data;
    ConfigManager config_manager;
    config_manager.wait_before_sleep = false;
    config_manager.ignore_audio = false;
    char before_cmds[][3] = {"b1", "b2"};
    char after_cmds[][3] = {"a1", "a2"};
    // It shouldn't matter if the timeouts are passed in non-sorted order
    int64_t timeouts[] = {3000, 2000};
    // add both timeouts
    config_manager.timeout_commands.emplace_back(make_command(before_cmds[0], after_cmds[0], timeouts[0]));
    config_manager.timeout_commands.emplace_back(make_command(before_cmds[1], after_cmds[1], timeouts[1]));
    EventManager event_manager(&config_manager);
    event_manager_init(event_manager);
    g_assert_cmpuint(activity_detector.cb_data.size(), ==, 2);
    // trigger each timeout one at a time
    event_manager.receive(EVENT_ACTIVITY_TIMEOUT, activity_detector.cb_data[1]);
    g_assert_cmpuint(process_spawner.async_cmds.size(), ==, 1);
    if (num_timeouts == 2) {
        event_manager.receive(EVENT_ACTIVITY_TIMEOUT, activity_detector.cb_data[0]);
        g_assert_cmpuint(process_spawner.async_cmds.size(), ==, 2);
    }
    // verify that the timeouts were executed in order of increasing timeout
    g_assert_cmpstr(process_spawner.async_cmds.at(0), ==, before_cmds[1]);
    if (num_timeouts == 2) {
        g_assert_cmpstr(process_spawner.async_cmds.at(1), ==, before_cmds[0]);
    }
    // now resume activity
    event_manager.receive(EVENT_ACTIVITY_RESUME, NULL);
    // verify that the "after" commands were run for the timeouts which went off
    g_assert_cmpuint(process_spawner.async_cmds.size(), ==, 2 * num_timeouts);
    if (num_timeouts == 1) {
        g_assert_cmpstr(process_spawner.async_cmds.at(1), ==, after_cmds[1]);
    } else {
        g_assert_cmpstr(process_spawner.async_cmds.at(2), ==, after_cmds[0]);
        g_assert_cmpstr(process_spawner.async_cmds.at(3), ==, after_cmds[1]);
    }
}

static void test_sleep_1(gpointer, gconstpointer user_data) {
    ConfigManager config_manager;
    config_manager.wait_before_sleep = (bool)user_data;
    config_manager.ignore_audio = false;
    config_manager.disable_automatic_dpms_activation  = false;
    config_manager.disable_screensaver = false;
    config_manager.sleep_commands.emplace_back(make_command("s1", "w1", 0, Command::SLEEP));
    EventManager event_manager(&config_manager);
    event_manager_init(event_manager);
    // send a SLEEP signal
    event_manager.receive(EVENT_SLEEP, NULL);
    if (config_manager.wait_before_sleep) {
        g_assert_cmpuint(process_spawner.sync_cmds.size(), ==, 1);
        g_assert_cmpstr(process_spawner.sync_cmds.at(0), ==, "s1");
    } else {
        g_assert_cmpuint(process_spawner.async_cmds.size(), ==, 1);
        g_assert_cmpstr(process_spawner.async_cmds.at(0), ==, "s1");
    }
    // send a WAKE signal
    event_manager.receive(EVENT_WAKE, NULL);
    if (config_manager.wait_before_sleep) {
        g_assert_cmpuint(process_spawner.async_cmds.size(), ==, 1);
        g_assert_cmpstr(process_spawner.async_cmds.at(0), ==, "w1");
    } else {
        g_assert_cmpuint(process_spawner.async_cmds.size(), ==, 2);
        g_assert_cmpstr(process_spawner.async_cmds.at(1), ==, "w1");
    }
}

static void test_audio_1(gpointer, gconstpointer user_data) {
    ConfigManager config_manager;
    config_manager.wait_before_sleep = false;
    config_manager.ignore_audio = (bool)user_data;
    config_manager.timeout_commands.emplace_back(make_command("b1", "a1", 2000));
    EventManager event_manager(&config_manager);
    event_manager_init(event_manager);
    if (config_manager.ignore_audio) {
        g_assert_cmpint(audio_detector.initialized, ==, 0);
        return;
    }

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
    ConfigManager config_manager;
    config_manager.wait_before_sleep = false;
    config_manager.ignore_audio = false;
    config_manager.idlehint_timeout_sec = 2;
    EventManager event_manager(&config_manager);
    event_manager_init(event_manager);
    // verify that a timeout was registered
    g_assert_cmpuint(activity_detector.cb_data.size(), ==, 1);
    event_manager.receive(EVENT_ACTIVITY_TIMEOUT, activity_detector.cb_data[0]);
    // IdleHint should be set to true
    g_assert_cmpint(logind_manager.idle_hint_history.at(0), ==, true);
    event_manager.receive(EVENT_ACTIVITY_RESUME, activity_detector.cb_data[0]);
    // IdleHint should be set to false
    g_assert_cmpint(logind_manager.idle_hint_history.at(1), ==, false);
}

static void test_lock(gpointer, gconstpointer) {
    ConfigManager config_manager;
    config_manager.wait_before_sleep = false;
    config_manager.ignore_audio = false;
    config_manager.lock_commands.emplace_back(make_command("l1", "u1", 0, Command::LOCK));
    EventManager event_manager(&config_manager);
    event_manager_init(event_manager);

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
