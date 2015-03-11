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

# engine
suites += abbrevs commands completion keys options parsing variables
# io
suites += ioeta ionotif iop ior
# ui
suites += colmgr column_view viewcolumns_parser
# everything else
suites += env escape fileops filetype filter misc undo utils

# obtain list of sources that are being tested
vifm_src := ./ cfg/ compat/ engine/ io/ io/private/ modes/dialogs/ menus/ modes/
vifm_src += ui/ utils/
vifm_src := $(wildcard $(addprefix ../src/, $(addsuffix *.c, $(vifm_src))))

# filter out generally non-testable or sources for another platform
vifm_src := $(filter-out %/vifm.c %/vifmrc-converter.c %/win_helper.c, \
                         $(vifm_src))
ifndef unix_env
    vifm_src := $(filter-out %/desktop.c %/mntent.c %_nix.c, $(vifm_src))
endif
ifndef win_env
    vifm_src := $(filter-out %/wcwidth.c %/volumes_menu.c %_win.c, $(vifm_src))
endif

ifdef DEBUG
BINSUBDIR := debug
endif

vifm_bin := ../src/vifm$(exe_suffix)
vifm_obj := $(vifm_src:.c=.o) bin/build/$(BINSUBDIR)/stubs.o

ifdef win_env
    make_arg   := -f Makefile.win
    exe_suffix := .exe
endif

# setup compile and link flags (partially depends on OS)
CFLAGS := -MMD -pipe -Wall -Werror -Istic/ -DTEST -include ../config.h
CFLAGS += -D_XOPEN_SOURCE -Wno-char-subscripts
LDFLAGS := -lpthread
ifdef unix_env
    LDFLAGS += $(shell sed -n     '/LIBS =/{s/^[^=]*=//p;q}' ../src/Makefile)
    LDFLAGS += $(shell sed -n  '/LDFLAGS =/{s/^[^=]*=//p;q}' ../src/Makefile)
    CFLAGS  += $(shell sed -n '/CPPFLAGS =/{s/^[^=]*=//p;q}' ../src/Makefile)
    CFLAGS  += -I/usr/include/ncursesw
endif
ifdef win_env
    LDFLAGS += $(shell sed -n '/LIBS :=/{s/^[^=]\+=//p;q}' ../src/Makefile.win)
endif

ifdef DEBUG
CFLAGS += -g
LDFLAGS += -g
endif

.PHONY: check build clean $(suites)

# check and build targets are defined mostly in suite_template
check: build

clean:
	-$(RM) -r bin/ stic/stic.h.d stic/stic.h.gch

$(vifm_src:.c=.o): | $(vifm_bin)

$(vifm_bin): $(vifm_src)
	$(MAKE) -C ../src/ $(make_arg) vifm$(exe_suffix)

bin/build/$(BINSUBDIR)/stubs.o: stubs.c
	gcc -c -o $@ $(CFLAGS) $<

bin/build/$(BINSUBDIR)/stic.o: stic/stic.c
	gcc -c -o $@ $(CFLAGS) $<

stic/stic.h.gch: stic/stic.h
	gcc -c -o $@ $(CFLAGS) $<

bin/$(BINSUBDIR)/:
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

# suite definition template, takes single argument: name of the suite
define suite_template

$1.src := $$(sort $$(wildcard $1/*.c))
$1.obj := $$($1.src:%.c=bin/build/$(BINSUBDIR)/%.o)

deps += $$($1.obj:.o=.d)

bin/$(BINSUBDIR)/$1$(exe_suffix): $$($1.obj) bin/build/$(BINSUBDIR)/stic.o \
                                  $(vifm_obj) | $(vifm_bin) bin/$(BINSUBDIR)/
	gcc -o $$@ $$^ $(LDFLAGS)

bin/build/$(BINSUBDIR)/$1/%.o: $1/%.c $1/. stic/stic.h.gch \
                             | bin/build/$(BINSUBDIR)/$1
	gcc -c -o $$@ $(CFLAGS) -include stic/stic.h \
	                        -DTESTID=$$(call pos, $$($1.obj), $$@) \
	                        -DMAXTESTID=$$(words $$($1.obj)) $$<

bin/build/$(BINSUBDIR)/$1:
	mkdir -p $$@

$1: bin/$(BINSUBDIR)/$1$(exe_suffix)
	@$$^

build: bin/$(BINSUBDIR)/$1$(exe_suffix)

check: $1

endef

# walk throw list of suites and instantiate template for each one
$(foreach suite, $(suites), $(eval $(call suite_template,$(suite))))

# import dependencies calculated by the compiler
include $(wildcard $(deps) bin/build/stic.d stic/stic.h.d bin/build/stubs.d)
