## Process this file with automake to produce Makefile.in

.PHONY: update_compile_info

AUTOMAKE_OPTIONS = subdir-objects

pkgdata_DATA = ../data/vifmrc \
							 ../data/vifm-help.txt

bin_PROGRAMS = vifm vifmrc-converter

bin_SCRIPTS = vifm-pause

man_MANS = ../data/vifm.1 \
					 ../data/vifm-pause.1 \
					 ../data/vifmrc-converter.1

MOSTLYCLEANFILES = compile_info.c \
									 ../data/vifm-help.txt

desktopappdir = $(datadir)/applications
desktopappfile = ../data/vifm.desktop
desktopapp_DATA = $(desktopappfile)

desktoppixdir = $(datadir)/pixmaps
desktoppixfile = ../data/graphics/vifm.png
desktoppix_DATA = $(desktoppixfile)

vifm_SOURCES = \
	\
	cfg/config.c cfg/config.h \
	cfg/hist.c cfg/hist.h \
	cfg/info.c cfg/info.h \
	\
	engine/cmds.c engine/cmds.h \
	engine/completion.c engine/completion.h \
	engine/functions.c engine/functions.h \
	engine/keys.c engine/keys.h \
	engine/options.c engine/options.h \
	engine/parsing.c engine/parsing.h \
	engine/text_buffer.c engine/text_buffer.h \
	engine/var.c engine/var.h \
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
	utils/filter.c utils/filter.h \
	utils/fs.c utils/fs.h \
	utils/int_stack.c utils/int_stack.h \
	utils/log.c utils/log.h \
	utils/macros.h \
	utils/mntent.c utils/mntent.h \
	utils/path.c utils/path.h \
	utils/str.c utils/str.h \
	utils/string_array.c utils/string_array.h \
	utils/tree.c utils/tree.h \
	utils/utf8.c utils/utf8.h \
	utils/utils.c utils/utils.h \
	utils/utils_nix.c utils/utils_nix.h \
	\
	background.c background.h \
	bookmarks.c bookmarks.h \
	bracket_notation.c bracket_notation.h \
	builtin_functions.c builtin_functions.h \
	color_scheme.c colorscheme.h \
	column_view.c column_view.h \
	colors.h \
	color_manager.c color_manager.h \
	commands.c commands.h \
	commands_completion.c commands_completion.h \
	desktop.c desktop.h \
	dir_stack.c dir_stack.h \
	escape.c escape.h \
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
	path_env.c path_env.h \
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
	types.c types.h \
	ui.c ui.h \
	undo.c undo.h \
	version.c version.h \
	viewcolumns_parser.c viewcolumns_parser.h \
	compile_info.c \
	vifm.c

vifmrc_converter_SOURCES = \
	color_scheme.h \
	vifmrc-converter.c

check_SCRIPTS = runtests
TESTS = $(check_SCRIPTS)

runtests:
	echo '$(MAKE) -C ../tests' > $@
	chmod +x $@

CLEANFILES = runtests

distclean-local:
	$(MAKE) -C ../tests clean

compile_info.c: update_compile_info

update_compile_info:
	@before="$$(cat "compile_info.c" 2> /dev/null)"; \
	if [ "x$$(which git 2> /dev/null)" != "x" ]; then \
		unset GIT_DIR; \
		new="const char GIT_HASH[] = \"$$(git rev-parse HEAD)\";"; \
	else \
		new='const char GIT_HASH[] = "";'; \
	fi; \
	if [ "x$$before" != "x$$new" -o ! -f "compile_info.c" ]; then \
		echo "$$new" > compile_info.c; \
	fi

../data/vifm-help.txt: ../data/vifm.1
	@if [ "x$$(whereis groff)" != "x" -a "x$$(whereis sed)" != "x" ]; then \
		groff -Tutf8 -man ../data/vifm.1 | sed -e 's/.\x08//g' -e 's/\x1b\[[0-9]*m//g' -e 's/\r//g' >| '$@'; \
	fi

tags.c: ../data/vim/doc/vifm.txt
	@if [ "x$$(whereis vim)" != "x" -a "x$$(whereis cat)" != "x" -a "x$$(whereis awk)" != "x" ]; then \
		vim -c 'helptags $(dir $<)|q' < /dev/tty; \
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
