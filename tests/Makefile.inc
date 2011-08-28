TEST_NAME := $(notdir $(abspath .))
ifneq ($(OS),Windows_NT)
    BIN := ../bin/$(TEST_NAME).exe
else
    BIN := ../bin/$(TEST_NAME).exe
endif

SOURCES += ../seatest/seatest.c
LOCAL_SOURCES += $(wildcard *.c)

OBJECTS := $(SOURCES:.c=.o)
LOCAL_OBJECTS := $(LOCAL_SOURCES:.c=.o)

ifneq ($(OS),Windows_NT)
    CFLAGS := -MD -g -Werror -I../seatest `pkg-config --cflags gtk+-2.0` -DTEST
    LDFLAGS := `pkg-config --libs gtk+-2.0` -lncursesw -lmagic
else
    CFLAGS := -MD -g -Werror -I../seatest -DTEST
    LDFLAGS := -lpdcurses -lregex
endif

$(BIN): $(addprefix ../bin/, $(notdir $(OBJECTS))) \
        $(addprefix bin/, $(notdir $(LOCAL_OBJECTS)))
	gcc -o $@ $^ $(LDFLAGS)

bin/%.o: %.c
	gcc -c -o $@ $< $(CFLAGS)

../bin/%.o: ../../src/%.c
	gcc -c -o $@ $< $(CFLAGS)

../bin/%.o: ../seatest/%.c
	gcc -c -o $@ $< $(CFLAGS)

include $(wildcard ../bin/*.d) $(wildcard bin/*.d)
