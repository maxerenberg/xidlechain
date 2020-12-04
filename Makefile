CXX = g++
CXXFLAGS = -Wall -Wno-write-strings `pkg-config --cflags gdk-2.0 gio-unix-2.0` -iquote ./
LIBS = `pkg-config --libs gdk-2.0 gio-unix-2.0 xext`
OBJECTS = xidlechain.o event_manager.o activity_manager.o logind_manager.o
TEST_OBJECTS = tests/activity_manager_test.o tests/logind_manager_test.o
PREFIX = ~/.local

.PHONY: all clean manpage install uninstall

all: xidlechain

xidlechain: ${OBJECTS}
	${CXX} -o xidlechain ${OBJECTS} ${LIBS}

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

tests: ${TEST_OBJECTS} activity_manager.o logind_manager.o
	${CXX} -o tests/activity_manager_test \
		tests/activity_manager_test.o activity_manager.o ${LIBS}
	${CXX} -o tests/logind_manager_test \
		tests/logind_manager_test.o logind_manager.o ${LIBS}

clean:
	rm -f xidlechain *.o xidlechain.1
	rm -f tests/*_test tests/*.o
