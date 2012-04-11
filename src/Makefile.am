## Process this file with automake to produce Makefile.in

.PHONY: tests version_hash.c

AUTOMAKE_OPTIONS = subdir-objects

pkgdata_DATA = ../data/vifmrc \
							 ../data/vifm-help.txt

bin_PROGRAMS = vifm vifmrc-converter

bin_SCRIPTS = vifm-pause

man_MANS = ../data/vifm.1

vifm_SOURCES = \
	\
	cfg/config.c cfg/config.h \
	cfg/info.c cfg/info.h \
	\
	engine/cmds.c engine/cmds.h \
	engine/completion.c engine/completion.h \
	engine/keys.c engine/keys.h \
	engine/options.c engine/options.h \
	engine/variables.c engine/variables.h \
	\
	menus/all.h \
	menus/apropos_menu.c menus/apropos_menu.h \
	menus/bookmarks_menu.c menus/bookmarks_menu.h \
	menus/colorscheme_menu.c menus/colorscheme_menu.h \
	menus/commands_menu.c menus/commands_menu.h \
	menus/dirhistory_menu.c menus/dirhistory_menu.h \
	menus/dirstask_menu.c menus/dirstask_menu.h \
	menus/filetypes_menu.c menus/filetypes_menu.h \
	menus/find_menu.c menus/find_menu.h \
	menus/grep_menu.c menus/grep_menu.h \
	menus/history_menu.c menus/history_menu.h \
	menus/jobs_menu.c menus/jobs_menu.h \
	menus/locate_menu.c menus/locate_menu.h \
	menus/map_menu.c menus/map_menu.h \
	menus/menus.c menus/menus.h \
	menus/registers_menu.c menus/registers_menu.h \
	menus/undolist_menu.c menus/undolist_menu.h \
	menus/users_menu.c menus/users_menu.h \
	menus/vifm_menu.c menus/vifm_menu.h \
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
	utils/env.c utils/env.h \
	utils/file_streams.c utils/file_streams.h \
	utils/fs.c utils/fs.h \
	utils/log.c utils/log.h \
	utils/macros.h \
	utils/path.c utils/path.h \
	utils/str.c utils/str.h \
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
	desktop.c desktop.h \
	dir_stack.c dir_stack.h \
	globals.c globals.h \
	file_magic.c file_magic.h \
	filelist.c filelist.h \
	filename_modifiers.c filename_modifiers.h \
	fileops.c fileops.h \
	filetype.c filetype.h \
	fuse.c fuse.h \
	ipc.c ipc.h \
	macros.c macros.h \
	main_loop.c main_loop.h \
	ops.c ops.h \
	opt_handlers.c opt_handlers.h \
	quickview.c quickview.h \
	registers.c registers.h \
	running.c running.h \
	search.c search.h \
	signals.c signals.h \
	sort.c sort.h \
	status.c status.h \
	tags.c tags.h \
	term_title.c term_title.h \
	trash.c trash.h \
	ui.c ui.h \
	undo.c undo.h \
	version.c version.h \
	version_hash.c \
	vifm.c

vifmrc_converter_SOURCES = \
	color_scheme.h \
	vifmrc-converter.c

tests:
	make -C ../tests

version_hash.c:
	@if [ "x$$(which git)" != "x" ]; then \
		echo "const char GIT_HASH[] = \"$$(git rev-parse HEAD)\";" > $@; \
	else \
		echo 'const char GIT_HASH[] = "";' > $@; \
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
