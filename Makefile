CXX = g++
EXT_DEPS = gdk-x11-3.0 gio-unix-2.0 gudev-1.0 xext libpulse libpulse-mainloop-glib
# don't include all the GLib headers inside the .d files
CXXFLAGS = -Wall -MMD -std=c++17 -iquote ./ \
	$(patsubst -I%,-isystem %,$(shell pkg-config --cflags $(EXT_DEPS)))
LDLIBS = $(shell pkg-config --libs $(EXT_DEPS))
COMMON_OBJECTS = event_manager.o activity_detector.o logind_manager.o \
	audio_detector.o process_spawner.o command.o config_manager.o \
	brightness_controller.o
OBJECTS = xidlechain.o $(COMMON_OBJECTS)
DEPENDS = ${OBJECTS:.o=.d}
PREFIX = ~/.local

.PHONY: all clean manpage install uninstall

all: xidlechain

xidlechain: ${OBJECTS}
	${CXX} -o $@ $^ ${LDLIBS}

${OBJECTS}: Makefile  # recompile if this file changed

manpage:
	scdoc < xidlechain.1.scd > xidlechain.1

install: xidlechain manpage
	install -D -t ${PREFIX}/bin xidlechain
	install -D -t ${PREFIX}/share/man/man1 xidlechain.1
	mandb -u -q

uninstall:
	rm -f ${PREFIX}/bin/xidlechain
	rm -f ${PREFIX}/share/man/man1/xidlechain.1
	mandb -u -q

activity_detector_test: tests/activity_detector_test.o activity_detector.o
	${CXX} -o tests/$@ $^ `pkg-config --libs gdk-x11-3.0 xext`

logind_manager_test: tests/logind_manager_test.o logind_manager.o
	${CXX} -o tests/$@ $^ `pkg-config --libs gio-unix-2.0`

audio_detector_test: tests/audio_detector_test.o audio_detector.o
	${CXX} -o tests/$@ $^ `pkg-config --libs libpulse libpulse-mainloop-glib`

event_manager_test: tests/event_manager_test.o event_manager.o config_manager.o command.o
	${CXX} -o tests/$@ $^ `pkg-config --libs glib-2.0`

tests: activity_detector_test logind_manager_test audio_detector_test event_manager_test

-include ${DEPENDS}

clean:
	rm -f xidlechain xidlechain.1 *.o *.d
	rm -f tests/*_test tests/*.o
