## Process this file with automake to produce Makefile.in

.PHONY: version.c

pkgdata_DATA = ../data/vifmrc \
							 ../data/vifm-help.txt

bin_PROGRAMS = vifm vifmrc-converter

bin_SCRIPTS = vifm-pause

man_MANS = ../data/vifm.1

vifm_SOURCES = \
	attr_dialog_nix.c attr_dialog_nix.h attr_dialog.h \
	background.c background.h \
	bookmarks.c bookmarks.h \
	change_dialog.c change_dialog.h \
	cmdline.c cmdline.h \
	cmds.c cmds.h \
	color_scheme.c colorscheme.h \
	commands.c commands.h \
	completion.c completion.h \
	config.c config.h \
	dir_stack.c dir_stack.h \
	file_info.c file_info.h \
	file_magic.c file_magic.h \
	filelist.c filelist.h \
	fileops.c fileops.h \
	filetype.c filetype.h \
	ipc.c ipc.h \
	keys.c keys.h \
	log.c log.h \
	main_loop.c main_loop.h \
	menu.c menu.h \
	menus.c menus.h \
	modes.c modes.h \
	normal.c normal.h \
	ops.c ops.h \
	opt_handlers.c opt_handlers.h \
	options.c options.h \
	registers.c registers.h \
	search.c search.h \
	signals.c signals.h \
	sort.c sort.h \
	sort_dialog.c sort_dialog.h \
	status.c status.h \
	string_array.c string_array.h \
	tags.c tags.h \
	trash.c trash.h \
	tree.c tree.h \
	ui.c ui.h \
	undo.c undo.h \
	utf8.c utf8.h \
	utils.c utils.h \
	variables.c variables.h \
	version.c version.h \
	view.c view.h \
	vifm.c \
	visual.c visual.h

vifmrc_converter_SOURCES = \
	color_scheme.h \
	vifmrc-converter.c

utils.o: version.o

version.c:
	@if [ "x$$(whereis git)" != "x" ] ; then \
		/bin/echo -n 'const char GIT_HASH[] = "' > version.c; \
		/bin/echo -n `git rev-parse HEAD` >> version.c; \
		/bin/echo '";' >> version.c; \
	else \
		/bin/echo -n 'const char GIT_HASH[] = "";' > version.c; \
	fi

../data/vifm-help.txt: ../data/vifm.1
	@if [ "x$$(whereis groff)" != "x" -a "x$$(whereis sed)" != "x" ]; then \
		groff -Tutf8 -man ../data/vifm.1 | sed 's/.\x08//g' >| ../data/vifm-help.txt; \
	fi

tags.c: ../data/vim/doc/vifm.txt
	@if [ "x$$(whereis vim)" != "x" -a "x$$(whereis cat)" != "x" -a "x$$(whereis awk)" != "x" ]; then \
		vim -c 'helptags $(dir $<)|q'; \
		cat $(dir $<)/tags | awk ' \
			BEGIN { \
				print "const char *tags[] = {" \
			} \
			{ \
				print "\t\"" $$1 "\"," \
			} \
			END { \
				print "\t0,\n};" \
			} \
		' > $@; \
	fi

# vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab :
