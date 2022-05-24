CC = gcc
CXX = g++
EXT_DEPS = gtk+-3.0 gudev-1.0
# don't include all the GLib headers inside the .d files
CFLAGS = -Wall -MMD -iquote . -iquote .. \
	 $(patsubst -I%,-isystem %,$(shell pkg-config --cflags $(EXT_DEPS)))
CXXFLAGS = $(CFLAGS) -std=c++17
LDLIBS = $(shell pkg-config --libs $(EXT_DEPS))
AUTOGEN_C_FILES = resources.c
AUTOGEN_OBJECTS = ${AUTOGEN_C_FILES:.c=.o}
PARENT_OBJECTS = command.o errors.o
OBJECTS = main.o xidlechain_app.o xidlechain_app_window.o xidlechain_app_controller.o \
	  xidlechain_app_edit_action.o \
	  $(addprefix ../,$(PARENT_OBJECTS)) $(AUTOGEN_OBJECTS)
DEPENDS = ${OBJECTS:.o=.d}

.PHONY: all autogen clean

all: xidlechain-app

xidlechain-app: $(AUTOGEN_C_FILES) $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS) $(LDLIBS)

$(AUTOGEN_C_FILES): xidlechain_app.gresource.xml window.ui edit_action.ui styles.css
	glib-compile-resources $< --target=$@ --sourcedir=. --generate-source

autogen: $(AUTOGEN_C_FILES)

-include ${DEPENDS}

clean:
	rm -f xidlechain-app *.o *.d
	rm -f $(AUTOGEN_C_FILES)
