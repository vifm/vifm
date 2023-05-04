## Process this file with automake to produce Makefile.in

.PHONY: update_compile_info

AUTOMAKE_OPTIONS = subdir-objects

vim_dir = $(pkgdatadir)/vim
sample_colors_dir = $(pkgdatadir)/colors
vimdoc_dir = $(pkgdatadir)/vim-doc
global_colors_dir = $(sysconfdir)/vifm/colors

vim_autoload_vifm_dir = $(vim_dir)/autoload/vifm
vim_doc_dir = $(vim_dir)/doc
vim_ftdetect_dir = $(vim_dir)/ftdetect
vim_ftplugin_dir = $(vim_dir)/ftplugin
vim_plugin_dir = $(vim_dir)/plugin
vim_syntax_dir = $(vim_dir)/syntax

vimdoc_doc_dir = $(vimdoc_dir)/doc

dist_vim_autoload_vifm__DATA = ../data/vim/autoload/vifm/colorconv.vim \
															 ../data/vim/autoload/vifm/edit.vim \
															 ../data/vim/autoload/vifm/globals.vim
dist_sample_colors__DATA = ../data/colors/astrell-root.vifm \
									 ../data/colors/astrell-user.vifm \
									 ../data/colors/dmilith-root.vifm \
									 ../data/colors/dmilith-user.vifm \
									 ../data/colors/istib-solarized-dark.vifm \
									 ../data/colors/juef-zenburn.vifm \
									 ../data/colors/reicheltd-light.vifm \
									 ../data/colors/Default-256.vifm
dist_vim_doc__DATA = ../data/vim/doc/plugin/vifm-plugin.txt
nodist_vim_doc__DATA = $(top_srcdir)/data/vim/doc/plugin/tags
dist_vim_ftdetect__DATA = ../data/vim/ftdetect/vifm.vim \
										 ../data/vim/ftdetect/vifm-rename.vim
dist_vim_ftplugin__DATA = ../data/vim/ftplugin/vifm.vim \
										 ../data/vim/ftplugin/vifm-cmdedit.vim \
										 ../data/vim/ftplugin/vifm-edit.vim \
										 ../data/vim/ftplugin/vifm-rename.vim \
										 ../data/vim/ftplugin/mail_vifm.vim
dist_vim_plugin__DATA = ../data/vim/plugin/vifm.vim
dist_vim_syntax__DATA = ../data/vim/syntax/vifm.vim
dist_global_colors__DATA = ../data/colors/Default-256.vifm

dist_vimdoc_doc__DATA = ../data/vim/doc/app/vifm-app.txt \
												../data/vim/doc/app/vifm-lua.txt
nodist_vimdoc_doc__DATA = $(top_srcdir)/data/vim/doc/app/tags

dist_pkgdata_DATA = ../data/vifmrc@DATA_SUFFIX@
dist_pkgdata_SCRIPTS = ../data/vifm-media@DATA_SUFFIX@
nodist_pkgdata_DATA = ../data/vifm-help.txt

bin_PROGRAMS = vifm

dist_bin_SCRIPTS = vifm-pause vifm-screen-split vifm-convert-dircolors

dist_man_MANS = ../data/man/vifm.1 \
					 ../data/man/vifm-pause.1 \
					 ../data/man/vifm-screen-split.1 \
					 ../data/man/vifm-convert-dircolors.1

MOSTLYCLEANFILES = compile_info.c \
									 ../data/vifm-help.txt \
									 $(top_srcdir)/data/vim/doc/app/tags \
									 $(top_srcdir)/data/vim/doc/plugin/tags
clean-local:
	if [ '$(builddir)' != '$(srcdir)' ]; then \
		$(RM) tags.c; \
	fi

desktopappdir = $(datadir)/applications
desktopappfile = ../data/vifm.desktop
dist_desktopapp_DATA = $(desktopappfile)

desktoppixdir = $(datadir)/pixmaps
desktoppixfile = ../data/graphics/vifm.png
dist_desktoppix_DATA = $(desktoppixfile)

hicolor128dir = $(datadir)/icons/hicolor/128x128/apps
hicolor128file = ../data/graphics/vifm.png
dist_hicolor128_DATA = $(hicolor128file)

hicolorsvgdir = $(datadir)/icons/hicolor/scalable/apps
hicolorsvgfile = ../data/graphics/vifm.svg
dist_hicolorsvg_DATA = $(hicolorsvgfile)

bashcompldir = $(datadir)/bash-completion/completions
bashcomplfile = ../data/shell-completion/bash/vifm
dist_bashcompl_DATA = $(bashcomplfile)

zshcompldir = $(datadir)/zsh/site-functions
zshcomplfile = ../data/shell-completion/zsh/_vifm
dist_zshcompl_DATA = $(zshcomplfile)

fishcompldir = $(datadir)/fish/vendor_completions.d
fishcomplfile = ../data/shell-completion/fish/vifm.fish
dist_fishcompl_DATA = $(fishcomplfile)

docsfile = ../AUTHORS ../BUGS ../COPYING ../ChangeLog ../ChangeLog.LuaAPI \
					 ../FAQ ../INSTALL ../NEWS ../README ../TODO
docsdir = $(docdir)
dist_docs_DATA = $(docsfile)

vifm_SOURCES = \
	\
	cfg/config.c cfg/config.h \
	cfg/info.c cfg/info.h \
	cfg/info_chars.h \
	\
	compat/curses.c compat/curses.h \
	compat/dtype.c compat/dtype.h \
	compat/fs_limits.h \
	compat/getopt.c compat/getopt1.c compat/getopt.h compat/getopt_int.h \
	compat/mntent.c compat/mntent.h \
	compat/os.c compat/os.h \
	compat/pthread.c compat/pthread.h \
	compat/reallocarray.c compat/reallocarray.h \
	\
	engine/abbrevs.c engine/abbrevs.h \
	engine/autocmds.c engine/autocmds.h \
	engine/cmds.c engine/cmds.h \
	engine/completion.c engine/completion.h \
	engine/functions.c engine/functions.h \
	engine/keys.c engine/keys.h \
	engine/mode.c engine/mode.h \
	engine/options.c engine/options.h \
	engine/parsing.c engine/parsing.h \
	engine/text_buffer.c engine/text_buffer.h \
	engine/var.c engine/var.h \
	engine/variables.c engine/variables.h \
	\
	int/desktop.c int/desktop.h \
	int/ext_edit.c int/ext_edit.h \
	int/file_magic.c int/file_magic.h \
	int/fuse.c int/fuse.h \
	int/path_env.c int/path_env.h \
	int/term_title.c int/term_title.h \
	int/vim.c int/vim.h \
	\
	io/ioc.h \
	io/ioe.h \
	io/ioe.c io/ioe.h \
	io/ioeta.c io/ioeta.h \
	io/ionotif.h \
	io/iop.c io/iop.h \
	io/ior.c io/ior.h \
	io/private/ioc.c io/private/ioc.h \
	io/private/ioe.c io/private/ioe.h \
	io/private/ioeta.c io/private/ioeta.h \
	io/private/ionotif.c io/private/ionotif.h \
	io/private/traverser.c io/private/traverser.h \
	\
	lua/lua/lapi.c lua/lua/lapi.h \
	lua/lua/lauxlib.c lua/lua/lauxlib.h \
	lua/lua/lbaselib.c \
	lua/lua/lcode.c lua/lua/lcode.h \
	lua/lua/lcorolib.c \
	lua/lua/lctype.c lua/lua/lctype.h \
	lua/lua/ldblib.c \
	lua/lua/ldebug.c lua/lua/ldebug.h \
	lua/lua/ldo.c lua/lua/ldo.h \
	lua/lua/ldump.c \
	lua/lua/lfunc.c lua/lua/lfunc.h \
	lua/lua/lgc.c lua/lua/lgc.h \
	lua/lua/linit.c \
	lua/lua/liolib.c \
	lua/lua/ljumptab.h \
	lua/lua/llex.c lua/lua/llex.h \
	lua/lua/llimits.h \
	lua/lua/lmathlib.c \
	lua/lua/lmem.c lua/lua/lmem.h \
	lua/lua/loadlib.c \
	lua/lua/lobject.c lua/lua/lobject.h \
	lua/lua/lopcodes.c lua/lua/lopcodes.h \
	lua/lua/lopnames.h \
	lua/lua/loslib.c \
	lua/lua/lparser.c lua/lua/lparser.h \
	lua/lua/lprefix.h \
	lua/lua/lstate.c lua/lua/lstate.h \
	lua/lua/lstring.c lua/lua/lstring.h \
	lua/lua/lstrlib.c \
	lua/lua/ltable.c lua/lua/ltable.h \
	lua/lua/ltablib.c \
	lua/lua/ltm.c lua/lua/ltm.h \
	lua/lua/lua.h \
	lua/lua/luaconf.h \
	lua/lua/lualib.h \
	lua/lua/lundump.c lua/lua/lundump.h \
	lua/lua/lutf8lib.c \
	lua/lua/lvm.c lua/lua/lvm.h \
	lua/lua/lzio.c lua/lua/lzio.h \
	\
	lua/api.h \
	lua/common.c lua/common.h \
	lua/vifm.c lua/vifm.h \
	lua/vifm_cmds.c lua/vifm_cmds.h \
	lua/vifm_events.c lua/vifm_events.h \
	lua/vifm_handlers.c lua/vifm_handlers.h \
	lua/vifm_keys.c lua/vifm_keys.h \
	lua/vifm_tabs.c lua/vifm_tabs.h \
	lua/vifm_viewcolumns.c lua/vifm_viewcolumns.h \
	lua/vifmentry.c lua/vifmentry.h \
	lua/vifmjob.c lua/vifmjob.h \
	lua/vifmtab.c lua/vifmtab.h \
	lua/vifmview.c lua/vifmview.h \
	lua/vlua.c lua/vlua.h \
	lua/vlua_cbacks.c lua/vlua_cbacks.h \
	lua/vlua_state.c lua/vlua_state.h \
	\
	menus/all.h \
	menus/apropos_menu.c menus/apropos_menu.h \
	menus/bmarks_menu.c menus/bmarks_menu.h \
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
	menus/marks_menu.c menus/marks_menu.h \
	menus/media_menu.c menus/media_menu.h \
	menus/menus.c menus/menus.h \
	menus/plugins_menu.c menus/plugins_menu.h \
	menus/registers_menu.c menus/registers_menu.h \
	menus/undolist_menu.c menus/undolist_menu.h \
	menus/users_menu.c menus/users_menu.h \
	menus/vifm_menu.c menus/vifm_menu.h \
	menus/volumes_menu.h \
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
	modes/more.c modes/more.h \
	modes/normal.c modes/normal.h \
	modes/view.c modes/view.h \
	modes/visual.c modes/visual.h \
	modes/wk.h \
	\
	ui/cancellation.c ui/cancellation.h \
	ui/color_manager.c ui/color_manager.h \
	ui/color_scheme.c ui/color_scheme.h \
	ui/colored_line.c ui/colored_line.h \
	ui/colors.h \
	ui/column_view.c ui/column_view.h \
	ui/escape.c ui/escape.h \
	ui/fileview.c ui/fileview.h \
	ui/private/statusline.h \
	ui/quickview.c ui/quickview.h \
	ui/statusbar.c ui/statusbar.h \
	ui/statusline.c ui/statusline.h \
	ui/tabs.c ui/tabs.h \
	ui/ui.c ui/ui.h \
	\
	utils/cancellation.c utils/cancellation.h \
	utils/darray.h \
	utils/dynarray.c utils/dynarray.h \
	utils/env.c utils/env.h \
	utils/file_streams.c utils/file_streams.h \
	utils/filemon.c utils/filemon.h \
	utils/filter.c utils/filter.h \
	utils/fs.c utils/fs.h \
	utils/fsdata.c utils/fsdata.h utils/private/fsdata.h \
	utils/fsddata.c utils/fsddata.h \
	utils/fswatch_nix.c utils/fswatch.h \
	utils/globs.c utils/globs.h \
	utils/gmux_nix.c utils/gmux.h \
	utils/hist.c utils/hist.h \
	utils/int_stack.c utils/int_stack.h \
	utils/log.c utils/log.h \
	utils/macros.h \
	utils/matcher.c utils/matcher.h \
	utils/matchers.c utils/matchers.h \
	utils/mem.c utils/mem.h \
	utils/parson.c utils/parson.h \
	utils/path.c utils/path.h \
	utils/regexp.c utils/regexp.h \
	utils/selector_nix.c utils/selector.h \
	utils/shmem_nix.c utils/shmem.h \
	utils/str.c utils/str.h \
	utils/string_array.c utils/string_array.h \
	utils/test_helpers.h \
	utils/trie.c utils/trie.h \
	utils/utf8.c utils/utf8.h \
	utils/utils.c utils/utils.h \
	utils/utils_int.h \
	utils/utils_nix.c utils/utils_nix.h \
	utils/xxhash.h \
	\
	args.c args.h \
	background.c background.h \
	bmarks.c bmarks.h \
	bracket_notation.c bracket_notation.h \
	builtin_functions.c builtin_functions.h \
	cmd_completion.c cmd_completion.h \
	cmd_core.c cmd_core.h \
	cmd_handlers.c cmd_handlers.h \
	compare.c compare.h \
	dir_stack.c dir_stack.h \
	event_loop.c event_loop.h \
	filelist.c filelist.h \
	filename_modifiers.c filename_modifiers.h \
	fops_common.c fops_common.h \
	fops_cpmv.c fops_cpmv.h \
	fops_misc.c fops_misc.h \
	fops_put.c fops_put.h \
	fops_rename.c fops_rename.h \
	filetype.c filetype.h \
	filtering.c filtering.h \
	flist_hist.c flist_hist.h \
	flist_pos.c flist_pos.h \
	flist_sel.c flist_sel.h \
	instance.c instance.h \
	ipc.c ipc.h \
	macros.c macros.h \
	marks.c marks.h \
	ops.c ops.h \
	opt_handlers.c opt_handlers.h \
	plugins.c plugins.h \
	registers.c registers.h \
	running.c running.h \
	search.c search.h \
	signals.c signals.h \
	sort.c sort.h \
	status.c status.h \
	tags.c tags.h \
	trash.c trash.h \
	types.c types.h \
	undo.c undo.h \
	vcache.c vcache.h \
	version.c version.h \
	viewcolumns_parser.c viewcolumns_parser.h \
	vifm.c vifm.h
nodist_vifm_SOURCES = \
	compile_info.c

check_SCRIPTS = runtests
TESTS = $(check_SCRIPTS)

EXTRA_DIST = \
						 update-compile-info \
						 helpztags \
						 menus/volumes_menu.c \
						 vifm.rc \
						 win_helper.c \
						 utils/fswatch_win.c \
						 utils/gmux_win.c \
						 utils/selector_win.c \
						 utils/shmem_win.c \
						 utils/utils_win.c \
						 utils/utils_win.h \
						 utils/xxhash.c \
						 modes/dialogs/attr_dialog_win.h \
						 modes/dialogs/attr_dialog_win.c \
						 compat/wcwidth.c \
						 compat/wcwidth.h \
						 Makefile.win \
						 lcov.css \
						 lcovrc \
						 ../data/graphics/vifm.ico \
						 ../data/graphics/vifm.svg \
						 ../data/vifmrc \
						 ../data/vifmrc-osx \
						 ../data/vifm-media \
						 ../data/vifm-media-osx

MANGEN = @MANGEN_PROG@
COL = @COL_PROG@
AWK = @AWK_PROG@
PERL = @PERL_PROG@
SED = @SED_PROG@
VIM = @VIM_PROG@
UNDER_VCS = @IN_GIT_REPO@

runtests:
	$(AM_V_GEN)echo 'mkdir -p "$(abs_top_builddir)/tests/"' > $@_ && \
	echo \
		'$(MAKE) -C "$(abs_top_srcdir)/tests" B="$(abs_top_builddir)/tests/" CC="$(CC)" "$$@"' \
		>> $@_ && \
	chmod +x $@_ && \
	mv $@_ $@

coverage: check
	lcov --directory $(builddir)/ --base-directory . \
	     --capture --output-file $(builddir)/lcov-all.info --config-file \
	     $(srcdir)/lcovrc --test-name unit_tests --quiet
	lcov --remove $(builddir)/lcov-all.info -o $(builddir)/lcov.info \
	     '/usr/*'
	genhtml --output-directory $(builddir)/cov-report/ \
	     $(builddir)/lcov.info --config-file $(srcdir)/lcovrc \
	     --show-details --css-file $(srcdir)/lcov.css

CLEANFILES = runtests lcov-all.info lcov.info $(vifm_OBJECTS:.o=.gcno) \
                                              $(vifm_OBJECTS:.o=.gcda)

distclean-local:
	$(MAKE) -C "$(abs_top_srcdir)/tests" B="$(abs_top_builddir)/tests/" clean
	$(RM) -r cov-report/

version.o: $(vifm_OBJECTS:version.o=)
compile_info.c: update_compile_info
	@$(srcdir)/update-compile-info $(UNDER_VCS)

# No action needed for this target.
update_compile_info:

../data/vifm-help.txt: $(top_srcdir)/data/man/vifm.1
	$(AM_V_GEN)mkdir -p ../data/; \
	if [ -n "$(MANGEN)" -a -n "$(SED)" ]; then \
		if [ -n "$(COL)" ]; then \
			$(MANGEN) -Tascii -man $(top_srcdir)/data/man/vifm.1 | $(SED) -e 's/\x1b\[[0-9]*m//g' -e 's/\x0d//g' | $(COL) -b >| '$@'; \
		else \
			$(MANGEN) -Tascii -man $(top_srcdir)/data/man/vifm.1 | $(SED) -e 's/.\x08//g' -e 's/\x1b\[[0-9]*m//g' -e 's/\x0d//g' >| '$@'; \
		fi \
	fi

$(top_srcdir)/data/vim/doc/app/tags: \
                                   $(top_srcdir)/data/vim/doc/app/vifm-app.txt \
                                   $(top_srcdir)/data/vim/doc/app/vifm-lua.txt
	$(AM_V_GEN)mkdir -p ../data/vim/doc/app/; \
	if [ -n "$(PERL)" ]; then \
		$(srcdir)/helpztags "$(top_srcdir)/data/vim/doc/app"; \
	elif [ -n "$(VIM)" ]; then \
		vim -e -s -c 'helptags $(top_srcdir)/data/vim/doc/app|q'; \
	else \
		echo "Can't generate tags without perl or vim"; \
		false; \
	fi

$(top_srcdir)/data/vim/doc/plugin/tags: \
                               $(top_srcdir)/data/vim/doc/plugin/vifm-plugin.txt
	$(AM_V_GEN)mkdir -p ../data/vim/doc/plugin/; \
	if [ -n "$(PERL)" ]; then \
		$(srcdir)/helpztags "$(top_srcdir)/data/vim/doc/plugin"; \
	elif [ -n "$(VIM)" ]; then \
		vim -e -s -c 'helptags $(top_srcdir)/data/vim/doc/plugin|q'; \
	else \
		echo "Can't generate tags without perl or vim"; \
		false; \
	fi

tags.c: $(top_srcdir)/data/vim/doc/app/tags
	$(AM_V_GEN)if [ -s $(top_srcdir)/data/vim/doc/app/tags -a -n "$(AWK)" ]; then \
		$(AWK) ' \
			BEGIN { \
				print "const char *tags[] = {" \
			} \
			{ \
				print "\t\"" $$1 "\"," \
			} \
			END { \
				print "\t0,\n};" \
			} \
		' $(top_srcdir)/data/vim/doc/app/tags > $@; \
	elif [ '$(builddir)' != '$(srcdir)' ]; then \
		echo 'const char *tags[] = {};' > $@; \
	fi

# vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab :
