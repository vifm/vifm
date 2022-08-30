# Overview
# --------
#
# This is a non-recursive Makefile for running vifm tests using stic.
#
# Vifm is built before running any tests as its object files are used for tests.
# Necessary include and link directives are picked up from the main Makefile.
#
# Each test-suite (directory) is run in a separate sandbox directory.  During
# compilation two defines are available:
#  - TEST_DATA_PATH -- path to test-data/ directory (might be RO)
#  - SANDBOX_PATH   -- path to sandbox for the suite (always RW)
#
# Test suites can be run concurrently.
#
# Usage
# -----
#
# By default tests are built in release mode.
#
# make              -- builds and runs all tests
# make build        -- builds all tests without running them
# make check        -- builds all tests and then runs them
# make <dir>        -- runs specific test suite
# make <dir>.<name> -- runs specific fixture
#
# make DEBUG=1 ...        -- builds debug version
# make DEBUG=gdb ...      -- builds debug version and loads suite into gdb
# make DEBUG=rr ...       -- builds debug version and loads suite into rr
# make DEBUG=valgrind ... -- builds debug version and runs it under valgrind
#                            (see valgrind-report file after a run)
#
# make VERBOSE=1 ... -- enables verbose printing of commands
#
# make TEST_RUN_PREFIX=wine -- sets run command prefix
#
# make clean -- removes various build artifacts
#
# make TESTS_CFLAGS=... TESTS_LDFLAGS... -- prepend something to CFLAGS/LDFLAGS
#
# "B" variable might be set to build tree root to run tests out of the source
# tree.

# don't pass $TERM to tests, they should work without a terminal and this
# variable
unexport TERM
# hide terminal multiplexers and graphic systems, so tests behave consistently
unexport STY TMUX DISPLAY WAYLAND_DISPLAY

# determine kind of OS that is running
ifeq ($(OS),Windows_NT)
    ifeq ($(shell uname -o 2> /dev/null),Cygwin)
        unix_env := true
    else
        win_env := true
    endif
else
    ifeq ($(CROSS),)
        ifeq ($(shell id -u),0)
            $(warning warning: running tests as root disables chmod-based ones)
        endif
        unix_env := true
    else
        win_env := true
    endif
endif

ifdef DEBUG
    BINSUBDIR := debug/
endif

# path to build tree
B ?=
# path to storage for intermediate build files
BUILD := $(B)bin/build/$(BINSUBDIR)

# engine
suites += abbrevs autocmds cmds commands completion keys options parsing
suites += text_buffer variables
# io
suites += ioeta ionotif iop ior
# ui
suites += colmgr column_view viewcolumns_parser
# everything else
suites += bmarks env escape fileops filetype filter lua misc undo utils

# these are built, but not automatically executed
apps := fuzz regs_shmem_app

# obtain list of sources that are being tested
vifm_src := ./ cfg/ compat/ engine/ int/ io/ io/private/ lua/ lua/lua/ menus/
vifm_src += modes/ modes/dialogs/ ui/ utils/
vifm_src := $(wildcard $(addprefix ../src/, $(addsuffix *.c, $(vifm_src))))
vifm_src := $(filter-out %/tags.c %/xxhash.c, $(vifm_src))

# filter out generally non-testable or sources for another platform
vifm_src := $(filter-out %/vifm.c %/win_helper.c, $(vifm_src))
ifndef unix_env
    vifm_src := $(filter-out %/desktop.c %/media_menu.c %/mntent.c %_nix.c, \
                             $(vifm_src))
endif
ifndef win_env
    vifm_src := $(filter-out %/wcwidth.c %/volumes_menu.c %_win.c, $(vifm_src))
endif

vifm_obj := $(B)../src/./tags.o $(vifm_src:%.c=$(B)%.o)

tests_lib := $(BUILD)libtest-support.a
tests_lib_src := $(wildcard test-support/*.c)
tests_lib_obj := $(patsubst %.c,$(BUILD)%.o,$(tests_lib_src))

# make sure that there is one compile_info.c object file in the list
vifm_obj := $(filter-out %/compile_info.o, $(vifm_obj))
vifm_obj += $(B)../src/compile_info.o

ifdef unix_env
    make_args := -C $(B)../src/
endif
ifdef win_env
    make_args  := -C $(B)../src/ -f $(abspath ../src/Makefile.win)
    exe_suffix := .exe
endif

CC = $(CROSS)gcc
AR = $(CROSS)ar

# handling of verbosity (non-verbose by default)
ifdef VERBOSE
    V = 1
else
    V = 0
endif
ACTUAL_CC := $(CC)
ACTUAL_AR := $(AR)
CC_0 = @echo "Compiling $@..."; $(ACTUAL_CC)
CC_1 = $(ACTUAL_CC)
AR_0 = @echo "Archiving $@..."; $(ACTUAL_AR)
AR_1 = $(ACTUAL_AR)
LD_0 = @echo "Linking $@..."; $(ACTUAL_CC)
LD_1 = $(ACTUAL_CC)
AT_0 = @
AT_1 =
# redefine commands according to verbosity state
override CC = $(CC_$(V))
override AR = $(AR_$(V))
override LD = $(LD_$(V))
AT = $(AT_$(V))

# setup compile and link flags (partially depends on OS)
override CFLAGS := $(TESTS_CFLAGS) -MMD -MP -pipe \
                   -Wall -Wno-char-subscripts \
                   -Itest-support/ -Itest-support/stic/ \
                   -include $(B)../build-aux/config.h \
                   -DTEST
override LDFLAGS := $(TESTS_LDFLAGS)
ifdef unix_env
    MF := $(abspath $(B)../src/Makefile)
    ifneq ($(wildcard $(MF)),)
        override LDFLAGS += \
            $(shell grep -m1    'LIBS =' $(MF) | sed 's/^[^=]*=//') \
            $(shell grep -m1 'LDFLAGS =' $(MF) | sed 's/^[^=]*=//')

        override CFLAGS  += \
            $(shell grep -m1          'CPPFLAGS =' $(MF) | sed 's/^[^=]*=//') \
            $(shell grep -m1      'TESTS_CFLAGS =' $(MF) | sed 's/^[^=]*=//') \
            $(shell grep -m1 'SANITIZERS_CFLAGS =' $(MF) | sed 's/^[^=]*=//')
    endif

    override CFLAGS += -I/usr/include/ncursesw
    export UBSAN_OPTIONS := halt_on_error=1
endif
ifdef win_env
    override CFLAGS += -pthread
    override LDFLAGS += \
        $(shell sed -n '/LIBS :=/{s/^[^=]\+=//p;q}' ../src/Makefile.win)
    # this part is in conditional of ../src/Makefile.win
    ifeq ($(OS),Windows_NT)
        override LDFLAGS += -lpdcurses
    else
        override LDFLAGS += -lcurses
    endif
endif
override CFLAGS += -fcommon
ifeq (,$(findstring -lpthread,$(LDFLAGS)))
    override LDFLAGS += -pthread
endif

# work around clang
ifeq (,$(findstring clang,$(shell $(ACTUAL_CC) --version)))
    ifneq (,$(findstring --coverage, $(LDFLAGS)))
        # clang is inconvenient with regard to this flag, don't do coverage with
        # it
        override CFLAGS += --coverage
        override LDFLAGS += --coverage
    endif

    # don't precompile header with clang (on OS X gcc is likely to be a symlink
    # to clang) because it handles macros in a different way
    GCH_DEP := $(BUILD)test-support/stic/stic.h.gch

    # this flag seems to be gcc-specific and clang freaks out on it with -Werror
    override CFLAGS += -Wno-format-truncation
endif

ifdef DEBUG
    override CFLAGS += -g -O0
    override LDFLAGS += -g
    ifdef unix_env
        override LDFLAGS += -rdynamic
    else
        override CFLAGS += -gdwarf-3
    endif

    ifeq ($(DEBUG),gdb)
        TEST_RUN_PREFIX := gdb -q --args
    endif
    ifeq ($(DEBUG),rr)
        TEST_RUN_PREFIX := rr record
    endif
    ifeq ($(DEBUG),valgrind)
        TEST_RUN_PREFIX := valgrind --track-origins=yes --leak-check=full \
                           --log-file=valgrind-report --error-exitcode=5 \
                           --suppressions=$(abspath .)/../src/.valgrind.supp
        TEST_RUN_POST := || (e=$$$$?; if [ $$$$e -eq 5 ]; then \
                                          cat valgrind-report; \
                                      fi; \
                                      exit $$$$e)
    endif
endif

.PHONY: check build clean $(suites) $(apps) appsources

# check and build targets are defined mostly in suite_template
check: build
	@if [ "$$(find "$(SANDBOX_PATH)" -mindepth 2 | wc -l)" -ne 0 ]; then \
	    echo "ERROR: sandbox isn't empty"; \
	    find $(SANDBOX_PATH) -mindepth 2; \
	    false; \
	fi

clean:
	$(RM) -r $(B)bin/ $(SANDBOX_PATH)/

# disable implicit rules to prevent compiling main sources here
.SUFFIXES:

# sources target groups dependencies to avoid invoking nested make per target
$(vifm_obj) ../src/./tags.c: appsources
	@# this receipt is needed to make Make recognize that targets were updated

appsources: $(vifm_src)
	$(MAKE) $(make_args) vifm$(exe_suffix)

$(BUILD)test-support/%.o: test-support/%.c | $(BUILD)test-support
	$(CC) -c -o $@ $(CFLAGS) $<

$(BUILD)test-support/stic/stic.o: test-support/stic/stic.c \
                                | $(BUILD)test-support/stic
	$(CC) -c -o $@ $(CFLAGS) $<

$(BUILD)test-support/stic/stic.h.gch: test-support/stic/stic.h \
                                    | $(BUILD)test-support/stic
	$(CC) -c -o $@ $(CFLAGS) $<

$(tests_lib): $(BUILD)test-support/stic/stic.o $(tests_lib_obj) \
            | $(BUILD)
	$(AR) cr $@ $^

$(BUILD) $(B)bin/$(BINSUBDIR) $(BUILD)test-support $(BUILD)test-support/stic:
	$(AT)mkdir -p $@

# this function of two arguments (array and element) returns index of the
# element in the array
pos = $(strip $(eval T := ) \
              $(eval i := 0) \
              $(foreach elem, $1, \
                        $(if $(filter $2,$(elem)), \
                             $(eval i := $(words $T)), \
                             $(eval T := $T $(elem)))) \
              $i)

ifeq (,$(findstring wine,$(TEST_RUN_PREFIX)))
    ifeq ($B,)
        SANDBOX_PATH := $(abspath .)/sandbox
    else
        SANDBOX_PATH := $(B)sandbox
    endif
    TEST_DATA_PATH := $(abspath .)/test-data
else
    SANDBOX_PATH := sandbox
    TEST_DATA_PATH := test-data
endif

ifneq ($(shell find "$(SANDBOX_PATH)" -mindepth 2 2> /dev/null | wc -l),0)
    $(shell $(RM) -r $(SANDBOX_PATH))
endif


# suite definition template, takes single argument: name of the suite
define suite_template

$1.src := $$(sort $$(wildcard $1/*.c))
$1.obj := $$($1.src:%.c=$(BUILD)%.o)
$1.bin := $(B)bin/$(BINSUBDIR)$1$(exe_suffix)
$1.fixtures := $$(subst /,.,\
                        $$(patsubst %.c,%,$$(filter-out %/suite.c,$$($1.src))))

deps += $$($1.obj:.o=.d)

$$($1.bin): $$($1.obj) $(vifm_obj) $(tests_lib) | $(B)bin/$(BINSUBDIR)
	$$(LD) -o $$@ $$^ $(LDFLAGS)

$(BUILD)$1/%.o: $1/%.c $(GCH_DEP) $(BUILD)$1/filelist \
              | $(BUILD)$1 $(SANDBOX_PATH)/$1
	$$(CC) -c -o $$@ $(CFLAGS) -include test-support/stic/stic.h \
	                 -DTESTID=$$(call pos, $$($1.obj), $$@) \
	                 -DMAXTESTID=$$(words $$($1.obj)) $$< \
	                 -DSUITE_NAME="$1" \
	                 -DTEST_DATA_PATH='"$(TEST_DATA_PATH)"' \
	                 -DSANDBOX_PATH='"$(SANDBOX_PATH)/$1"' \

$(BUILD)$1/filelist: $1/. | $(BUILD)$1
	@if [ ! -f "$$@" -o "$$$$(cat $$@ 2> /dev/null)" != '$$($1.src)' ]; then \
		echo -n '$$($1.src)' > $$@; \
	fi

$(BUILD)$1:
	$(AT)mkdir -p $$@

$(SANDBOX_PATH)/$1:
ifeq ($(SANDBOX_PATH),sandbox)
	$(AT)mkdir -p $(B)$$@
else
	$(AT)mkdir -p $$@
endif

$1: $$($1.bin)
ifeq ($B,)
	@$(TEST_RUN_PREFIX) $$< -s $(TEST_RUN_POST)
else
	@cd $B && $(TEST_RUN_PREFIX) $$< -s $(TEST_RUN_POST)
endif

# this runs separate fixtures, targets are of the form dir.name
.PHONY: $$($1.fixtures)
$$($1.fixtures): $$($1.bin)
ifeq ($B,)
	@$(TEST_RUN_PREFIX) $$^ -s -f $$(subst .,/,$$@).c $(TEST_RUN_POST)
else
	@cd $B && $(TEST_RUN_PREFIX) $$^ -s -f $$(subst .,/,$$@).c $(TEST_RUN_POST)
endif

build: $$($1.bin)

# don't add apps to the list of things to run automatically
ifeq (,$(filter $1,$(apps)))
check: $1
endif

endef

# walk throw list of suites+apps and instantiate template for each one
$(foreach suite, $(suites) $(apps), $(eval $(call suite_template,$(suite))))

# extra dependencies between test fixtures
misc: | $(regs_shmem_app.bin)

# import dependencies calculated by the compiler
include $(wildcard $(deps) \
                   $(BUILD)test-support/stic/stic.h.d \
                   $(BUILD)test-support/*.d)
