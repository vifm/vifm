SOURCES := ../../src/*.c ../../src/cfg/*.c ../../src/engine/*.c
SOURCES += ../../src/modes/dialogs/*.c ../../src/menus/*.c ../../src/modes/*.c
SOURCES += ../../src/utils/*.c

SOURCES := $(filter-out ../../src/vifm.c, $(wildcard $(SOURCES)))
SOURCES := $(filter-out ../../src/vifmrc-converter.c, $(SOURCES))
SOURCES := $(filter-out ../../src/win_helper.c, $(SOURCES))

ifneq ($(OS),Windows_NT)
    EXE_SUFFIX :=
else
    EXE_SUFFIX := .exe
endif

TEST_NAME := $(notdir $(abspath .))
BIN := ../bin/$(TEST_NAME)$(EXE_SUFFIX)

VIFM_BIN := ../../src/vifm$(EXE_SUFFIX)

SOURCES += ../seatest/seatest.c
ifeq ($(OS),Windows_NT)
    SOURCES := $(filter-out ../../src/desktop.c, $(SOURCES))
    SOURCES := $(filter-out ../../src/utils/mntent.c, $(SOURCES))
    SOURCES := $(filter-out %_nix.c, $(SOURCES))
else
    SOURCES := $(filter-out ../../src/menus/volumes_menu.c, $(SOURCES))
    SOURCES := $(filter-out %_win.c, $(SOURCES))
endif
LOCAL_SOURCES += $(wildcard *.c)

OBJECTS := $(SOURCES:.c=.o)
LOCAL_OBJECTS := $(LOCAL_SOURCES:.c=.o)
LOCAL_OBJECTS := $(addprefix bin/, $(notdir $(LOCAL_OBJECTS)))

CFLAGS := -MMD -g -Werror -I../seatest -DTEST -include ../../config.h
LDFLAGS := -lpthread
ifneq ($(OS),Windows_NT)
    LDFLAGS += `grep LIBS\ = ../../src/Makefile | sed 's/^[^=]\+=//'`
    LDFLAGS += `grep LDFLAGS\ = ../../src/Makefile | sed 's/^[^=]\+=//'`
    CFLAGS += `grep CPPFLAGS\ = ../../src/Makefile | sed 's/^[^=]\+=//'`
else
    LDFLAGS += `grep LIBS\ := ../../src/Makefile.win | sed 's/^[^=]\+=//'`
endif

ifneq ($(OS),Windows_NT)
    ifneq ($(wildcard /usr/include/ncursesw/.),)
        CFLAGS += -I/usr/include/ncursesw
    endif
endif

.PHONY: $(VIFM_BIN) clean

$(BIN): $(VIFM_BIN) ../bin/dummy $(OBJECTS) $(LOCAL_OBJECTS)
	gcc -o $@ $(OBJECTS) $(LOCAL_OBJECTS) $(LDFLAGS)

$(VIFM_BIN):
ifneq ($(OS),Windows_NT)
	$(MAKE) -C ../../src/ vifm
else
	$(MAKE) -C ../../src/ -f Makefile.win vifm.exe
endif

../bin/dummy: ../bin
	touch ../bin/dummy

bin/%.o: %.c bin/dummy
	gcc -c -o $@ $< $(CFLAGS)

bin/dummy: bin
	touch bin/dummy

../bin bin:
	mkdir $@

../bin/%.o: ../seatest/%.c
	gcc -c -o $@ $< $(CFLAGS)

clean:
	-$(RM) -r bin/

include $(wildcard ../bin/*.d) $(wildcard bin/*.d)

# vim: filetype=make :
