dirs := commands completion filetype keys misc options undo

.PHONY: tests $(dirs)

tests: $(dirs)
	for test in `find bin/ -executable -type f`; do\
		if ! $$test; then\
			exit 1;\
		fi;\
		echo;\
		echo;\
	done

$(dirs):
	make --directory=$@
