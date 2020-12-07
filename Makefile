CXX = g++
EXT_DEPS = gdk-2.0 gio-unix-2.0 xext libpulse libpulse-mainloop-glib
# don't include all the GLib headers inside the .d files
CXXFLAGS = -Wall -MMD -std=c++17 -Wno-write-strings -iquote ./ \
	$(patsubst -I%,-isystem %,$(shell pkg-config --cflags $(EXT_DEPS)))
LDLIBS = $(shell pkg-config --libs $(EXT_DEPS))
DETECTOR_OBJECTS = activity_manager.o logind_manager.o audio_manager.o
OBJECTS = xidlechain.o event_manager.o ${DETECTOR_OBJECTS}
TEST_OBJECTS = tests/activity_manager_test.o tests/logind_manager_test.o \
	tests/audio_manager_test.o
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
	install -D -t ${PREFIX}/bin/ xidlechain
	install -D -t ${PREFIX}/share/man/man1 xidlechain.1
	mandb -q

uninstall:
	rm -f ${PREFIX}/bin/xidlechain
	rm -f ${PREFIX}/share/man/man1/xidlechain.1
	mandb -q

tests: ${TEST_OBJECTS} ${DETECTOR_OBJECTS}
	${CXX} -o tests/activity_manager_test \
		tests/activity_manager_test.o activity_manager.o \
		`pkg-config --libs gdk-2.0 xext`
	${CXX} -o tests/logind_manager_test \
		tests/logind_manager_test.o logind_manager.o \
		`pkg-config --libs gio-unix-2.0`
	${CXX} -o tests/audio_manager_test \
		tests/audio_manager_test.o audio_manager.o \
		`pkg-config --libs glib-2.0` -lpulse -lpulse-mainloop-glib

-include ${DEPENDS}

clean:
	rm -f xidlechain xidlechain.1 *.o *.d
	rm -f tests/*_test tests/*.o
