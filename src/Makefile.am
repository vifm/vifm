## Process this file with automake to produce Makefile.in

.PHONY: update_compile_info

AUTOMAKE_OPTIONS = subdir-objects

vim_dir = $(pkgdatadir)/vim
vimdoc_dir = $(pkgdatadir)/vim-doc

vim_autoload_vifm_dir = $(vim_dir)/autoload/vifm
vim_doc_dir = $(vim_dir)/doc
vim_ftdetect_dir = $(vim_dir)/ftdetect
vim_ftplugin_dir = $(vim_dir)/ftplugin
vim_plugin_dir = $(vim_dir)/plugin
vim_syntax_dir = $(vim_dir)/syntax

vimdoc_doc_dir = $(vimdoc_dir)/doc

vim_autoload_vifm__DATA = ../data/vim/autoload/vifm/edit.vim
vim_doc__DATA = ../data/vim/doc/plugin/vifm-plugin.txt \
								../data/vim/doc/plugin/tags
vim_ftdetect__DATA = ../data/vim/ftdetect/vifm.vim \
										 ../data/vim/ftdetect/vifm-rename.vim
vim_ftplugin__DATA = ../data/vim/ftplugin/vifm.vim \
										 ../data/vim/ftplugin/vifm-cmdedit.vim \
										 ../data/vim/ftplugin/vifm-edit.vim \
										 ../data/vim/ftplugin/vifm-rename.vim \
										 ../data/vim/ftplugin/mail_vifm.vim
vim_plugin__DATA = ../data/vim/plugin/vifm.vim
vim_syntax__DATA = ../data/vim/syntax/vifm.vim

vimdoc_doc__DATA = ../data/vim/doc/app/vifm-app.txt \
									 ../data/vim/doc/app/tags

pkgdata_DATA = ../data/vifmrc \
							 ../data/vifmrc-osx \
							 ../data/vifm-help.txt

bin_PROGRAMS = vifm vifmrc-converter

bin_SCRIPTS = vifm-pause vifm-convert-dircolors

man_MANS = ../data/vifm.1 \
					 ../data/vifm-pause.1 \
					 ../data/vifmrc-converter.1 \
					 ../data/vifm-convert-dircolors.1

MOSTLYCLEANFILES = compile_info.c \
									 ../data/vifm-help.txt \
									 ../data/vim/doc/app/tags \
									 ../data/vim/doc/plugin/tags

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
	compat/getopt.c compat/getopt1.c compat/getopt.h compat/getopt_int.h \
	compat/os.c compat/os.h \
	\
	engine/abbrevs.c engine/abbrevs.h \
	engine/cmds.c engine/cmds.h \
	engine/completion.c engine/completion.h \
	engine/functions.c engine/functions.h \
	engine/keys.c engine/keys.h \
	engine/mode.c engine/mode.h \
	engine/options.c engine/options.h engine/private/options.h \
	engine/parsing.c engine/parsing.h \
	engine/text_buffer.c engine/text_buffer.h \
	engine/var.c engine/var.h \
	engine/variables.c engine/variables.h \
	\
	io/ioc.h \
	io/ioe.h \
	io/ioeta.c io/ioeta.h \
	io/iop.c io/iop.h \
	io/ior.c io/ior.h \
	io/private/ioeta.c io/private/ioeta.h \
	io/private/ionotif.c io/private/ionotif.h \
	io/private/traverser.c io/private/traverser.h \
	\
	menus/all.h \
	menus/apropos_menu.c menus/apropos_menu.h \
	menus/bookmarks_menu.c menus/bookmarks_menu.h \
	menus/cabbrevs_menu.c menus/cabbrevs_menu.h \
	menus/colorscheme_menu.c menus/colorscheme_menu.h \
	menus/commands_menu.c menus/commands_menu.h \
	menus/dirhistory_menu.c menus/dirhistory_menu.h \
	menus/dirstack_menu.c menus/dirstack_menu.h \
	menus/filetypes_menu.c menus/filetypes_menu.h \
	menus/find_menu.c menus/find_menu.h \
	menus/grep_menu.c menus/grep_menu.h \
	menus/history_menu.c menus/history_menu.h \
	menus/jobs_menu.c menus/jobs_menu.h \
	menus/locate_menu.c menus/locate_menu.h \
	menus/trash_menu.c menus/trash_menu.h \
	menus/trashes_menu.c menus/trashes_menu.h \
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
	modes/dialogs/msg_dialog.c modes/dialogs/msg_dialog.h \
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
	ui/cancellation.c ui/cancellation.h \
	ui/statusbar.c ui/statusbar.h \
	ui/statusline.c ui/statusline.h \
	ui/ui.c ui/ui.h \
	\
	utils/env.c utils/env.h \
	utils/file_streams.c utils/file_streams.h \
	utils/filemon.c utils/filemon.h \
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
	args.c args.h \
	background.c background.h \
	bookmarks.c bookmarks.h \
	bracket_notation.c bracket_notation.h \
	builtin_functions.c builtin_functions.h \
	color_scheme.c color_scheme.h \
	column_view.c column_view.h \
	colors.h \
	color_manager.c color_manager.h \
	commands.c commands.h \
	commands_completion.c commands_completion.h \
	desktop.c desktop.h \
	dir_stack.c dir_stack.h \
	escape.c escape.h \
	event_loop.c event_loop.h \
	globals.c globals.h \
	file_magic.c file_magic.h \
	filelist.c filelist.h \
	filename_modifiers.c filename_modifiers.h \
	fileops.c fileops.h \
	filetype.c filetype.h \
	fileview.c fileview.h \
	filtering.c filtering.h \
	fuse.c fuse.h \
	ipc.c ipc.h \
	macros.c macros.h \
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
	undo.c undo.h \
	version.c version.h \
	viewcolumns_parser.c viewcolumns_parser.h \
	compile_info.c \
	vifm.c vifm.h \
	vim.c vim.h

vifmrc_converter_SOURCES = \
	color_scheme.h \
	vifmrc-converter.c

check_SCRIPTS = runtests
TESTS = $(check_SCRIPTS)

runtests:
	echo '$(MAKE) -C ../tests' > $@_
	chmod +x $@_
	mv $@_ $@

CLEANFILES = runtests

distclean-local:
	$(MAKE) -C ../tests clean

version.o: $(filter-out version.o, $(vifm_OBJECTS))
compile_info.c: update_compile_info
	@./update-compile-info

# No action needed for this target.
update_compile_info:

../data/vifm-help.txt: ../data/vifm.1
	@if [ "x$$(which groff 2> /dev/null)" != "x" -a "x$$(which sed 2> /dev/null)" != "x" ]; then \
		if [ "x$$(which col 2> /dev/null)" != "x" ]; then \
			groff -Tutf8 -man ../data/vifm.1 | sed -e 's/\x1b\[[0-9]*m//g' -e 's/\x0d//g' | col -b >| '$@'; \
		else \
			groff -Tutf8 -man ../data/vifm.1 | sed -e 's/.\x08//g' -e 's/\x1b\[[0-9]*m//g' -e 's/\x0d//g' >| '$@'; \
		fi \
	fi

../data/vim/doc/app/tags: ../data/vim/doc/app/vifm-app.txt
	@if [ "x$$(which perl 2> /dev/null)" != "x" ]; then \
		./helpztags "$(dir $@)"; \
	elif [ "x$$(which vim 2> /dev/null)" != "x" ]; then \
		vim -e -s -c 'helptags $(dir $@)|q'; \
	else \
		touch $@; \
	fi

../data/vim/doc/plugin/tags: ../data/vim/doc/plugin/vifm-plugin.txt
	@if [ "x$$(which perl 2> /dev/null)" != "x" ]; then \
		./helpztags "$(dir $@)"; \
	elif [ "x$$(which vim 2> /dev/null)" != "x" ]; then \
		vim -e -s -c 'helptags $(dir $@)|q'; \
	else \
		touch $@; \
	fi

tags.c: ../data/vim/doc/app/tags
	@if [ -s "$<" -a "x$$(which cat)" != "x" -a "x$$(which awk)" != "x" ]; then \
		cat "$<" | awk ' \
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
