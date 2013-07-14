dirs := colmgr commands column_view completion env escape filetype filter keys
dirs += misc options parsing undo variables viewcolumns_parser
# generate names for build sub-targets (one for each directory)
dirs_b := $(addsuffix _b, $(dirs))
# generate names for clean sub-targets (one for each directory)
dirs_c := $(addsuffix _c, $(dirs))

.PHONY: tests clean $(dirs) $(dirs_b) $(dirs_c)

tests: bin $(dirs_b)
ifneq ($(OS),Windows_NT)
	@for test in `find bin/ -executable -type f`; do\
		echo '==================================================';\
		echo Test Suite: $$test;\
		echo '==================================================';\
		echo;\
		echo;\
		if ! $$test; then\
			exit 1;\
		fi;\
		echo;\
		echo;\
		echo;\
	done
else
	@for test in bin/*.exe; do\
		echo '==================================================';\
		echo Test Suite: $$test;\
		echo '==================================================';\
		echo;\
		echo;\
		if ! $$test; then\
			exit 1;\
		fi;\
		echo;\
		echo;\
		echo;\
	done
endif

$(dirs_b): bin
	$(MAKE) --directory=$(subst _b,,$@) --makefile=../Makefile.inc

$(dirs): bin
	$(MAKE) --directory=$@ --makefile=../Makefile.inc
ifneq ($(OS),Windows_NT)
	@test=bin/$@;\
	echo '==================================================';\
	echo Test Suite: $$test;\
	echo '==================================================';\
	$$test
else
	@test=bin/$@.exe;\
	echo '==================================================';\
	echo Test Suite: $$test;\
	echo '==================================================';\
	$$test
endif

bin:
	mkdir $@

clean: $(dirs_c)
	-$(RM) -r bin/
	-$(RM) seatest/*.o seatest/*.d

$(dirs_c):
	$(MAKE) --directory=$(subst _c,,$@) --makefile=../Makefile.inc clean
