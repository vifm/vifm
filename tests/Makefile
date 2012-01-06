dirs := commands completion filetype keys misc options undo variables

.PHONY: tests $(dirs)

tests: bin $(dirs)
ifneq ($(OS),Windows_NT)
	for test in `find bin/ -executable -type f`; do\
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
	for test in bin/*.exe; do\
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

$(dirs): bin
	make --directory=$@

bin:
	mkdir $@
