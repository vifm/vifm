## Process this file with automake to produce Makefile.in

pkgdata_DATA = vifmrc \
							 vifm-help.txt

bin_PROGRAMS = vifm vifmrc-converter

bin_SCRIPTS = vifm-pause

man_MANS = vifm.1

vifm_SOURCES = \
	background.c background.h \
	bookmarks.c bookmarks.h \
	cmdline.c cmdline.h \
	color_scheme.c colorscheme.h \
	cmds.c cmds.h \
	commands.c commands.h \
	completion.c completion.h \
	config.c config.h \
	dir_stack.c dir_stack.h \
	file_info.c file_info.h \
	file_magic.c file_magic.h \
	filelist.c filelist.h \
	fileops.c fileops.h \
	filetype.c filetype.h \
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
	tags.c tags.h \
	trash.c trash.h \
	tree.c tree.h \
	ui.c ui.h \
	undo.c undo.h \
	utils.c utils.h \
	version.c version.h \
	vifm.c \
	visual.c visual.h

vifmrc_converter_SOURCES = \
	color_scheme.h \
	vifmrc-converter.c

version.c:
	if [ "x$$(whereis git)" != "x" ] ; then \
		echo -n 'const char GIT_HASH[] = "' > version.c; \
		echo -n `git rev-parse HEAD` >> version.c; \
		echo '";' >> version.c; \
	else \
		echo -n 'const char GIT_HASH[] = "";' > version.c; \
	fi

# vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab :
