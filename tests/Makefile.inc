include ../detectenv.mk

SOURCES := ../../src/*.c ../../src/cfg/*.c ../../src/engine/*.c ../../src/io/*.c
SOURCES += ../../src/io/private/*.c ../../src/modes/dialogs/*.c
SOURCES += ../../src/menus/*.c ../../src/modes/*.c ../../src/utils/*.c

SOURCES := $(filter-out ../../src/vifm.c, $(wildcard $(SOURCES)))
SOURCES := $(filter-out ../../src/vifmrc-converter.c, $(SOURCES))
SOURCES := $(filter-out ../../src/win_helper.c, $(SOURCES))

ifdef WIN_ENV
    EXE_SUFFIX := .exe
endif

TEST_NAME := $(notdir $(abspath .))
BIN := ../bin/$(TEST_NAME)$(EXE_SUFFIX)

VIFM_BIN := ../../src/vifm$(EXE_SUFFIX)

SOURCES += ../seatest/seatest.c
ifdef UNIX_ENV
    SOURCES := $(filter-out ../../src/menus/volumes_menu.c, $(SOURCES))
    SOURCES := $(filter-out %_win.c, $(SOURCES))
else
    SOURCES := $(filter-out ../../src/desktop.c, $(SOURCES))
    SOURCES := $(filter-out ../../src/utils/mntent.c, $(SOURCES))
    SOURCES := $(filter-out %_nix.c, $(SOURCES))
endif
LOCAL_SOURCES += $(wildcard *.c) ../stubs.c

OBJECTS := $(SOURCES:.c=.o)
LOCAL_OBJECTS := $(LOCAL_SOURCES:.c=.o)
LOCAL_OBJECTS := $(addprefix bin/, $(notdir $(LOCAL_OBJECTS)))

CFLAGS := -MMD -g -Wall -Werror -I../seatest -DTEST -include ../../config.h
CFLAGS += -Wno-char-subscripts
LDFLAGS := -lpthread
ifdef UNIX_ENV
    LDFLAGS += $(shell grep 'LIBS =' ../../src/Makefile | sed 's/^[^=]*=//')
    LDFLAGS += $(shell grep 'LDFLAGS =' ../../src/Makefile | sed 's/^[^=]*=//')
    CFLAGS += $(shell grep 'CPPFLAGS =' ../../src/Makefile | sed 's/^[^=]*=//')
else
    LDFLAGS += `grep LIBS\ := ../../src/Makefile.win | sed 's/^[^=]\+=//'`
endif

ifdef UNIX_ENV
    ifneq ($(wildcard /usr/include/ncursesw/.),)
        CFLAGS += -I/usr/include/ncursesw
    endif
endif

.PHONY: clean

$(BIN): $(VIFM_BIN) ../bin/.timestamp $(OBJECTS) $(LOCAL_OBJECTS)
	gcc -o $@ $(OBJECTS) $(LOCAL_OBJECTS) $(LDFLAGS)

$(VIFM_BIN): $(SOURCES)
ifdef UNIX_ENV
	$(MAKE) -C ../../src/ vifm
else
	$(MAKE) -C ../../src/ -f Makefile.win vifm.exe
endif

bin/%.o: %.c bin/.timestamp
	gcc -c -o $@ $< $(CFLAGS)

bin/stubs.o: ../stubs.c bin/.timestamp
	gcc -c -o $@ $< $(CFLAGS)

../bin/.timestamp bin/.timestamp:
	mkdir -p $(dir $@)
	touch $@

../bin/%.o: ../seatest/%.c
	gcc -c -o $@ $< $(CFLAGS)

clean:
	-$(RM) -r bin/

include $(wildcard ../bin/*.d) $(wildcard bin/*.d)

# vim: filetype=make :
