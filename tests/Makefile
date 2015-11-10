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
#  - TEST_DATA_PATH -- absolute path to test-data/ directory (might be RO)
#  - SANDBOX_PATH   -- absolute path to sandbox for the suite (always RW)
#
# Test suites can be run concurrently.
#
# Usage
# -----
#
# By default tests are built in release mode.
#
# make              -- runs all tests
# make <dir>        -- runs specific test suite
# make <dir>.<name> -- runs specific fixture
#
# make DEBUG=1 ...        -- builds debug version
# make DEBUG=gdb ...      -- builds debug version and loads suite into gdb
# make DEBUG=valgrind ... -- builds debug version and run it under valgrind (see
#                            valgrind-report file after a run)
#
# make clean -- removes various built artifacts
#
# "B" variable might be set to run tests out of the source tree.

# determine kind of OS that is running
ifeq ($(OS),Windows_NT)
    ifeq ($(shell uname -o 2> /dev/null),Cygwin)
        unix_env := true
    else
        win_env := true
    endif
else
    unix_env := true
endif

# build directory
B ?=

# engine
suites += abbrevs commands completion keys options parsing variables
# io
suites += ioeta ionotif iop ior
# ui
suites += colmgr column_view viewcolumns_parser
# everything else
suites += bmarks env escape fileops filetype filter misc undo utils

# obtain list of sources that are being tested
vifm_src := ./ cfg/ compat/ engine/ int/ io/ io/private/ modes/dialogs/ menus/
vifm_src += modes/ ui/ utils/
vifm_src := $(wildcard $(addprefix ../src/, $(addsuffix *.c, $(vifm_src))))

# filter out generally non-testable or sources for another platform
vifm_src := $(filter-out %/vifm.c %/win_helper.c, $(vifm_src))
ifndef unix_env
    vifm_src := $(filter-out %/desktop.c %/mntent.c %_nix.c, $(vifm_src))
endif
ifndef win_env
    vifm_src := $(filter-out %/wcwidth.c %/volumes_menu.c %_win.c, $(vifm_src))
endif

ifdef DEBUG
BINSUBDIR := debug
endif

vifm_bin := $(B)../src/vifm$(exe_suffix)
vifm_obj := $(vifm_src:%.c=$(B)%.o) $(B)bin/build/$(BINSUBDIR)/stubs.o

# make sure that there is one compile_info.c object file in the list
vifm_obj := $(filter-out %/compile_info.o, $(vifm_obj))
vifm_obj += $(B)../src/compile_info.o

ifdef win_env
    make_arg   := -f Makefile.win
    exe_suffix := .exe
endif

CC := gcc

# setup compile and link flags (partially depends on OS)
CFLAGS := -MMD -pipe -Wall -Werror -Istic/ -DTEST -include $(B)../config.h
CFLAGS += -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED -Wno-char-subscripts
LDFLAGS := -lpthread --coverage
ifdef unix_env
    LDFLAGS += $(shell sed -n     '/LIBS =/{s/^[^=]*=//p;q}' $(B)../src/Makefile)
    LDFLAGS += $(shell sed -n  '/LDFLAGS =/{s/^[^=]*=//p;q}' $(B)../src/Makefile)
    CFLAGS  += $(shell sed -n '/CPPFLAGS =/{s/^[^=]*=//p;q}' $(B)../src/Makefile)
    CFLAGS  += -I/usr/include/ncursesw

    CFLAGS  += $(shell sed -n '/SANITIZERS_CFLAGS =/{s/^[^=]*=//p;q}' \
               $(B)../src/Makefile)
endif
ifdef win_env
    LDFLAGS += $(shell sed -n '/LIBS :=/{s/^[^=]\+=//p;q}' ../src/Makefile.win)
endif

ifdef DEBUG
    CFLAGS += -g
    LDFLAGS += -g
    ifdef unix_env
        LDFLAGS += -rdynamic
    endif

    ifeq ($(DEBUG),gdb)
        TEST_RUN_PREFIX := gdb --args
    endif
    ifeq ($(DEBUG),valgrind)
        TEST_RUN_PREFIX := valgrind --track-origins=yes --leak-check=full \
                           --log-file=valgrind-report
    endif
endif

.PHONY: check build clean $(suites)

# check and build targets are defined mostly in suite_template
check: build

clean:
	$(RM) -r $(B)bin/ $(B)sandbox/
	$(RM) $(B)stic/stic.h.d $(B)stic/stic.h.gch

$(vifm_src:.c=.o): | $(vifm_bin)

$(vifm_bin): $(vifm_src)
	$(MAKE) -C $(B)../src/ $(make_arg) vifm$(exe_suffix)

$(B)bin/build/$(BINSUBDIR)/stubs.o: stubs.c
	$(CC) -c -o $@ $(CFLAGS) $<

$(B)bin/build/$(BINSUBDIR)/stic.o: stic/stic.c
	$(CC) -c -o $@ $(CFLAGS) $<

$(B)stic/stic.h.gch: stic/stic.h | $(B)stic
	$(CC) -c -o $@ $(CFLAGS) $<

$(B)stic $(B)bin/$(BINSUBDIR)/:
	mkdir -p $@

# this function of two arguments (array and element) returns index of the
# element in the array
pos = $(strip $(eval T := ) \
              $(eval i := 0) \
              $(foreach elem, $1, \
                        $(if $(filter $2,$(elem)), \
                             $(eval i := $(words $T)), \
                             $(eval T := $T $(elem)))) \
              $i)

ifeq ($B,)
    SANDBOX_PATH := $(abspath .)/sandbox
else
    SANDBOX_PATH := $(B)sandbox
endif

TEST_DATA_PATH := $(abspath .)/test-data

# suite definition template, takes single argument: name of the suite
define suite_template

$1.src := $$(sort $$(wildcard $1/*.c))
$1.obj := $$($1.src:%.c=$(B)bin/build/$(BINSUBDIR)/%.o)
$1.bin := $(B)bin/$(BINSUBDIR)/$1$(exe_suffix)
$1.fixtures := $$(subst /,.,$$(patsubst %.c,%,$$($1.src)))

deps += $$($1.obj:.o=.d)

$$($1.bin): $$($1.obj) $(B)bin/build/$(BINSUBDIR)/stic.o $(vifm_obj) \
          | $(vifm_bin) $(B)bin/$(BINSUBDIR)/
	$(CC) -o $$@ $$^ $(LDFLAGS)

$(B)bin/build/$(BINSUBDIR)/$1/%.o: $1/%.c $(B)stic/stic.h.gch \
                                   $(B)bin/build/$(BINSUBDIR)/$1/filelist \
                                 | $(B)bin/build/$(BINSUBDIR)/$1 $(B)sandbox/$1
	$(CC) -c -o $$@ -include stic/stic.h $(CFLAGS) \
	                -DTESTID=$$(call pos, $$($1.obj), $$@) \
	                -DMAXTESTID=$$(words $$($1.obj)) $$< \
	                -DSUITE_NAME="$1" \
	                -DTEST_DATA_PATH='"$(TEST_DATA_PATH)"' \
	                -DSANDBOX_PATH='"$(SANDBOX_PATH)/$1"' \

$(B)bin/build/$(BINSUBDIR)/$1/filelist: $1/. | $(B)bin/build/$(BINSUBDIR)/$1
	@if [ ! -f "$$@" -o "$$$$(cat $$@ 2> /dev/null)" != '$$($1.src)' ]; then \
		echo -n '$$($1.src)'; \
		echo -n '$$($1.src)' > $$@; \
	fi

$(B)bin/build/$(BINSUBDIR)/$1 $(B)sandbox/$1:
	mkdir -p $$@

$1: $$($1.bin)
ifeq ($B,)
	@$(TEST_RUN_PREFIX) $$^ -s
else
	@cd $B && $(TEST_RUN_PREFIX) $$^ -s
endif

# this runs separate fixtures, targets are of the form dir.name
.PHONY: $$($1.fixtures)
$$($1.fixtures): $$($1.bin)
ifeq ($B,)
	@$(TEST_RUN_PREFIX) $$^ -s -f $$(subst .,/,$$@).c
else
	@cd $B && $(TEST_RUN_PREFIX) $$^ -s -f $$(subst .,/,$$@).c
endif

build: $$($1.bin)

check: $1

endef

# walk throw list of suites and instantiate template for each one
$(foreach suite, $(suites), $(eval $(call suite_template,$(suite))))

# import dependencies calculated by the compiler
include $(wildcard $(deps) \
                   $(B)bin/build/stic.d $(B)stic/stic.h.d $(B)bin/build/stubs.d)
