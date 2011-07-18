dirs := commands keys misc options undo

.PHONY: tests $(dirs)

tests: $(dirs)
	for test in bin/*; do\
		if ! $$test; then\
			exit -1;\
		fi;\
		echo;\
		echo;\
	done

$(dirs):
	make --directory=$@
