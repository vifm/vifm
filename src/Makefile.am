## Process this file with automake to produce Makefile.in

.PHONY: utils.c version.c

pkgdata_DATA = ../data/vifmrc \
							 ../data/vifm-help.txt

bin_PROGRAMS = vifm vifmrc-converter

bin_SCRIPTS = vifm-pause

man_MANS = ../data/vifm.1

vifm_SOURCES = \
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
	permissions_dialog.c permissions_dialog.h \
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
	version.c version.h \
	view.c view.h \
	vifm.c \
	visual.c visual.h

vifmrc_converter_SOURCES = \
	color_scheme.h \
	vifmrc-converter.c

version.c:
	if [ "x$$(whereis git)" != "x" ] ; then \
		/bin/echo -n 'const char GIT_HASH[] = "' > version.c; \
		/bin/echo -n `git rev-parse HEAD` >> version.c; \
		/bin/echo '";' >> version.c; \
	else \
		/bin/echo -n 'const char GIT_HASH[] = "";' > version.c; \
	fi

# vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab :
