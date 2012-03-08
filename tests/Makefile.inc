SOURCES := ../../src/*.c ../../src/engine/*.c ../../src/modes/dialogs/*.c
SOURCES += ../../src/modes/*.c ../../src/utils/*.c
SOURCES := $(filter-out ../../src/vifm.c, $(wildcard $(SOURCES)))
SOURCES := $(filter-out ../../src/vifmrc-converter.c, $(SOURCES))
SOURCES := $(filter-out ../../src/win_helper.c, $(SOURCES))

TEST_NAME := $(notdir $(abspath .))
ifneq ($(OS),Windows_NT)
    BIN := ../bin/$(TEST_NAME)
else
    BIN := ../bin/$(TEST_NAME).exe
endif

SOURCES += ../seatest/seatest.c
ifeq ($(OS),Windows_NT)
    DIALOG := ../../src/modes/dialogs/attr_dialog_nix.c
else
    DIALOG := ../../src/modes/dialogs/attr_dialog_win.c
endif
SOURCES := $(filter-out $(DIALOG), $(SOURCES))
LOCAL_SOURCES += $(wildcard *.c)

OBJECTS := $(SOURCES:.c=.o)
OBJECTS := $(addprefix ../bin/, $(notdir $(OBJECTS)))
LOCAL_OBJECTS := $(LOCAL_SOURCES:.c=.o)
LOCAL_OBJECTS := $(addprefix bin/, $(notdir $(LOCAL_OBJECTS)))

ifneq ($(OS),Windows_NT)
    CFLAGS := -MD -g -Werror -I../seatest `pkg-config --cflags gtk+-2.0` -DTEST
    LDFLAGS := `pkg-config --libs gtk+-2.0` -lncursesw -lmagic
else
    CFLAGS := -MD -g -Werror -I../seatest -DTEST
    LDFLAGS := -lpdcurses -lregex -lnetapi32 -lpthread -lws2_32
endif

$(BIN): ../bin/dummy $(OBJECTS) $(LOCAL_OBJECTS)
	gcc -o $@ $(OBJECTS) $(LOCAL_OBJECTS) $(LDFLAGS)

../bin/dummy:
	-mkdir ../bin
	touch ../bin/dummy

bin/%.o: %.c bin/dummy
	gcc -c -o $@ $< $(CFLAGS)

bin/dummy:
	-mkdir bin
	touch bin/dummy

../bin/%.o: ../../src/%.c
	gcc -c -o $@ $< $(CFLAGS)

../bin/%.o: ../seatest/%.c
	gcc -c -o $@ $< $(CFLAGS)

include $(wildcard ../bin/*.d) $(wildcard bin/*.d)

# vim: filetype=make :
