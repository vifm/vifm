dirs := commands completion filetype keys misc options undo

.PHONY: tests $(dirs)

tests: $(dirs)
ifneq ($(OS),Windows_NT)
	for test in `find bin/ -executable -type f`; do\
		if ! $$test; then\
			exit 1;\
		fi;\
		echo;\
		echo;\
	done
else
	for test in bin/*.exe; do\
		if ! $$test; then\
			exit 1;\
		fi;\
		echo;\
		echo;\
	done
endif

$(dirs):
	make --directory=$@
