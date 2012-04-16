dirs := commands completion filetype keys misc options parsing undo variables
dirs_s := $(addsuffix _t, $(dirs))

.PHONY: tests $(dirs) $(dirs_s)

tests: bin $(dirs_s)
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

$(dirs_s): bin
	make --directory=$(subst _t,,$@) --makefile=../Makefile.inc

$(dirs): bin
	make --directory=$@ --makefile=../Makefile.inc
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
