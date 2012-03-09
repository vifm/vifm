## Process this file with automake to produce Makefile.in

.PHONY: version.c

AUTOMAKE_OPTIONS = subdir-objects

pkgdata_DATA = ../data/vifmrc \
							 ../data/vifm-help.txt

bin_PROGRAMS = vifm vifmrc-converter

bin_SCRIPTS = vifm-pause

man_MANS = ../data/vifm.1

vifm_SOURCES = \
	\
	engine/cmds.c engine/cmds.h \
	engine/completion.c engine/completion.h \
	engine/keys.c engine/keys.h \
	engine/options.c engine/options.h \
	engine/variables.c engine/variables.h \
	\
	modes/dialogs/attr_dialog.h \
	modes/dialogs/attr_dialog_nix.c modes/dialogs/attr_dialog_nix.h \
	modes/dialogs/change_dialog.c modes/dialogs/change_dialog.h \
	modes/dialogs/sort_dialog.c modes/dialogs/sort_dialog.h \
	\
	modes/cmdline.c modes/cmdline.h \
	modes/file_info.c modes/file_info.h \
	modes/menu.c modes/menu.h \
	modes/modes.c modes/modes.h \
	modes/normal.c modes/normal.h \
	modes/view.c modes/view.h \
	modes/visual.c modes/visual.h \
	\
	utils/log.c utils/log.h \
	utils/macros.h \
	utils/string_array.c utils/string_array.h \
	utils/tree.c utils/tree.h \
	utils/utf8.c utils/utf8.h \
	utils/utils.c utils/utils.h \
	\
	background.c background.h \
	bookmarks.c bookmarks.h \
	color_scheme.c colorscheme.h \
	commands.c commands.h \
	commands_completion.c commands_completion.h \
	config.c config.h \
	dir_stack.c dir_stack.h \
	file_magic.c file_magic.h \
	filelist.c filelist.h \
	filename_modifiers.c filename_modifiers.h \
	fileops.c fileops.h \
	filetype.c filetype.h \
	fuse.c fuse.h \
	ipc.c ipc.h \
	main_loop.c main_loop.h \
	menus.c menus.h \
	ops.c ops.h \
	opt_handlers.c opt_handlers.h \
	registers.c registers.h \
	running.c running.h \
	search.c search.h \
	signals.c signals.h \
	sort.c sort.h \
	status.c status.h \
	tags.c tags.h \
	trash.c trash.h \
	ui.c ui.h \
	undo.c undo.h \
	version.c version.h \
	vifm.c

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
