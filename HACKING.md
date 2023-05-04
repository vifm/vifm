## Contributing ##

It's a good idea to describe what it is you would like to implement before
starting it.  The are multiple reasons for this:

 - new feature might interact with other features in unexpected ways;
 - not all ways of implementing a feature are equal, some might be preferred
   just for the sake of keeping things consistent;
 - the code base is quite large and stuff that's already there might save you
   some time.

Changes can be sent as a pull request on [github] or as a patch via [email].

Some information on development is available on the [wiki].

[github]: https://github.com/vifm/vifm/pulls
[email]: mailto:xaizek@posteo.net
[wiki]: https://wiki.vifm.info/index.php?title=Development#Code_repositories

## Setup for development ##

To avoid possible issues with `autoconf` caused by skewed timestamps after
`git clone` or `git checkout` execute `scripts/fix-timestamps`.

Configure the project with `--enable-developer` flag to compile in debug mode
and with `-Werror` (there is also `--enable-werror` that doesn't enable debug
mode).  Undo this if it breaks the build from `master` because of unexpected
warnings.

After making changes, don't forget to run tests (see below) to make sure they
pass.  It might be a good idea to start by running tests as well to see that
they succeed without any changes.

## Tests ##

Tests are available and can be run with `make check` in the root, `src/` or
`tests/` directory.  When inside `tests/`, there are multiple ways to run
specific tests in several different modes, see comment inside the `Makefile`
there for instructions.

## ChangeLog format ##

Releases are listed in newest to oldest order.  Beta versions are considered to
be separate releases, otherwise one would have to edit entries rather than add
new ones on recording changes done since beta.

Within each release entries are organized in groups of 4 kinds in this order:
 1. Changed ...
 2. Added ...
 3. Everything else
 4. Fixed ...

A new entry is added at the end of a group, resulting in a chronological order
of changes of each kind.

## Package contents ##

    .
    |
    |-- build-aux/ - part of autotools' machinery
    |
    |-- data/ - documentation, sample vifmrc, icons, etc.
    |  |
    |  |-- colors/ - sample color schemes
    |  |-- graphics/ - icons and screenshots
    |  |-- man/ - manual pages
    |  |-- shell-completion/ - scripts implementing shell completion
    |  `-- vim/ - plugin for Vim
    |
    |-- patches/ - patches for software vifm depends on
    |
    |-- pkgs/ - package build-scripts
    |
    |-- scripts/ - utility scripts for development
    |
    |-- src/ - the source code of vifm
    |  |
    |  |-- cfg/ - code related to configuration
    |  |  |
    |  |  |-- config.c - reads scripts and manages configuration
    |  |  |-- hist.c - convenient history list abstraction
    |  |  `-- info.c - read and write vifminfo
    |  |
    |  |-- compat/ - implementation of features that are missing on some OSes
    |  |  |
    |  |  |-- curses.c - emulation of some wide functions for OpenBSD
    |  |  |-- dentry.c - emulation of d_entry field of struct direntry
    |  |  |-- getopt.c - part of glibc to have getopt_long() everywhere
    |  |  |-- getopt1.c - getopt.c public interface implementation
    |  |  |-- mntent.c - compatibility file for FreeBSD-like systems
    |  |  |-- os.c - very thin abstraction layer over basic OS facilities
    |  |  |-- pthread.c - spinlock shim for OS X
    |  |  |-- reallocarray.c - more convenient and safe realloc() for arrays
    |  |  `-- wcwidth.c - wcwidth() implementation for Windows
    |  |
    |  |-- engine/ - the core of vi[m]-like functionality
    |  |  |
    |  |  |-- abbrevs.c - implementation of abbreviations
    |  |  |-- autocmds.c - implementation of autocommands
    |  |  |-- cmds.c - command line parsing core
    |  |  |-- completion.c - provides means to fill and get completion list
    |  |  |-- functions.c - provides support for builtin functions
    |  |  |-- keys.c - analyzes users input
    |  |  |-- mode.c - generic mode related routines
    |  |  |-- options.c - :set command
    |  |  |-- parsing.c - parses expressions
    |  |  |-- var.c - all needed to work with variables
    |  |  `-- variables.c - handles :let and :unlet commands
    |  |
    |  |-- int/ - integration with environment/external tools
    |  |  |
    |  |  |-- desktop.c - code that parses *.desktop files on *nix systems
    |  |  |-- ext_edit.c - managing of state related to external renaming
    |  |  |-- file_magic.c - determines programs associated with file using its
    |  |  |                  mime-type
    |  |  |-- fuse.c - provides support of FUSE filesystems
    |  |  |-- path_env.c - parses and modifies PATH environment variables
    |  |  |-- term_title.c - terminal title updates
    |  |  `-- vim.c - contains Vim integration functionality
    |  |
    |  |-- io/ - file system operations implementation
    |  |  |
    |  |  |-- private/ - internal part of i/o
    |  |  |  |
    |  |  |  |-- ioc.c - implementation of common i/o routines
    |  |  |  |-- ioeta.c - internal part of i/o estimations
    |  |  |  |-- ionotif.c - internal part of i/o notifications
    |  |  |  `-- traverser.c - file system traversing routine
    |  |  |
    |  |  |-- ioeta.c - i/o estimations management
    |  |  |-- iop.c - implementation of i/o primitive
    |  |  `-- ior.c - implementation of recursive i/o operations
    |  |
    |  |-- lua/ - Lua interface
    |  |  |
    |  |  |-- lua/ - Lua 5.4.4 sources
    |  |  |-- common.c - common code for Lua API implementation
    |  |  |-- vifm.c - implementation of `vifm`
    |  |  |-- vifm_cmds.c - implementation of `vifm.cmds`
    |  |  |-- vifm_events.c - implementation of `vifm.events`
    |  |  |-- vifm_handlers.c - implementation of `vifm.addhandler`
    |  |  |-- vifm_tabs.c - implementation of `vifm.tabs`
    |  |  |-- vifm_viewcolumns.c - implementation of `vifm.addcolumntype`
    |  |  |-- vifmentry.c - implementation of VifmEntry
    |  |  |-- vifmjob.c - implementation of VifmJob
    |  |  |-- vifmtab.c - implementation of VifmTab
    |  |  |-- vifmview.c - implementation of VifmView
    |  |  |-- vlua.c - main Lua interface unit
    |  |  |-- vlua_cbacks.c - manager of callback functions
    |  |  `-- vlua_state.c - management of state of vlua.c unit
    |  |
    |  |-- menus/ - implementation of all menus
    |  |  |
    |  |  |-- apropos_menu.c - handles :apropos menu
    |  |  |-- bmarks_menu.c - handles :bmarks menu
    |  |  |-- cabbrevs_menu.c - handles :cabbrev and :cnoreabbrev menus
    |  |  |-- colorscheme_menu.c - handles :colorscheme menu
    |  |  |-- commands_menu.c - handles :command menu
    |  |  |-- dirhistory_menu.c - handles :history menu
    |  |  |-- dirstack_menu.c - handles :dirs menu
    |  |  |-- filetypes_menu.c - handles :file menu
    |  |  |-- find_menu.c - handles :file menu
    |  |  |-- grep_menu.c - hanldes :grep menu
    |  |  |-- history_menu.c - handles :history command menus except directory
    |  |  |                    history
    |  |  |-- jobs_menu.c - handles :jobs menu
    |  |  |-- locate_menu.c - handles :locale menu
    |  |  |-- trash_menu.c - handles :lstrash menu
    |  |  |-- trashes_menu.c - handles :trashes menu
    |  |  |-- map_menu.c - handles :map menu
    |  |  |-- marks_menu.c - handles :marks menu
    |  |  |-- media.c - handles :media menu
    |  |  |-- menus.c - handles all kinds of menus
    |  |  |-- plugins_menu.c - handles :plugins menu
    |  |  |-- registers_menu.c - handles :registers menu
    |  |  |-- undolist_menu.c - handles :undolist menu
    |  |  |-- users_menu.c - handles menus created by %m or %M macros
    |  |  |-- vifm_menu.c - handles :vifm (or :version) menu
    |  |  `-- volumes_menu.c - handles :volumes menu on MS Windows systems
    |  |
    |  |-- modes/ - implementation of all modes
    |  |  |
    |  |  |-- dialogs/ - dialog modes
    |  |  |  |
    |  |  |  |-- attr_dialog_nix.c - file permissions dialog for *nix systems
    |  |  |  |-- attr_dialog_win.c - file properties dialog for MS Windows
    |  |  |  |                       systems
    |  |  |  |-- change_dialog.c - change dialog
    |  |  |  |-- msg_dialog.c - information/error/prompt messages
    |  |  |  `-- sort_dialog.c - dialog to choose sort type
    |  |  |
    |  |  |-- cmdline.c - command line mode
    |  |  |-- file_info.c - Control+G
    |  |  |-- menu.c - handles commands in menus
    |  |  |-- modes.c - general code (e.g. before and after key pressed) for
    |  |  |             modes
    |  |  |-- more.c - more mode that views too much output on status bar
    |  |  |-- normal.c - normal mode commands
    |  |  |-- view.c - view mode commands
    |  |  `-- visual.c - implementation of visual mode commands
    |  |
    |  |-- ui/ - most of ui related code
    |  |  |
    |  |  |-- private/ - internal headers of ui
    |  |  |-- cancellation.c - managing operation cancellation
    |  |  |-- color_manager.c - manager of curses color pairs
    |  |  |-- color_scheme.c - color schemes
    |  |  |-- colored_line.c - text with %x* highlighting
    |  |  |-- column_view.c - column formatting unit
    |  |  |-- escape.c - escape sequences related stuff
    |  |  |-- fileview.c - display/redraw/manage file view
    |  |  |-- quickview.c - implementation of quick view
    |  |  |-- statusbar.c - managing status bar
    |  |  |-- statusline.c - status line formatting
    |  |  |-- tabs.c - implementation of tabs
    |  |  `-- ui.c - ui initialization and other ui related functions
    |  |
    |  |-- utils/ - miscellaneous utility functions and data types
    |  |  |
    |  |  |-- private/ - internal headers of utilities
    |  |  |-- cancellation.c - kind of cancellation token
    |  |  |-- darray.c - macros for managin dynamic arrays
    |  |  |-- dynarray.c - array reallocation with fewer memory copies
    |  |  |-- env.c - environment variables related functions
    |  |  |-- fs.c - functions to deal with file system objects
    |  |  |-- fsdata.c - maps arbitrary data onto file system tree
    |  |  |-- fsddata.c - fsdata wrapper that takes care of dynamic memory
    |  |  |-- fswatch_nix.c - watches path in file system for changes on *nix
    |  |  |-- fswatch_win.c - watches path in file system for changes on Windows
    |  |  |-- file_streams.c - file stream reading related functions
    |  |  |-- filemon.c - file monitoring "object"
    |  |  |-- filter.c - small abstraction over filter driven by a regexp
    |  |  |-- globs.c - provides support of glob patterns
    |  |  |-- gmux_nix.c - implementation of named mutex on *nix
    |  |  |-- gmux_win.c - implementation of named mutex on Windows
    |  |  |-- int_stack.c - int stack "object"
    |  |  |-- log.c - primitive logging
    |  |  |-- matcher.c - file path/name matcher (glob/regexp/mime-type)
    |  |  |-- matchers.c - list of matchers (which are ANDed together)
    |  |  |-- mem.c - simple memory/array manipulation utilities
    |  |  |-- path.c - various functions to work with paths
    |  |  |-- regexp.c - regexp related
    |  |  |-- selector_nix.c - waiting for file descriptors to become readable
    |  |  |-- selector_win.c - waiting for file handles to become readable
    |  |  |-- shmem_nix.c - implementation of named shared memory on *nix
    |  |  |-- shmem_win.c - implementation of named shared memory on Windows
    |  |  |-- str.c - various string functions
    |  |  |-- string_array.c - functions to work with arrays of strings
    |  |  |-- trie.c - 3-way trie implementation
    |  |  |-- utf8.c - functions to handle utf8 strings
    |  |  |-- utils.c - various utilities
    |  |  |-- utils_nix.c - various utilities for *nix systems
    |  |  |-- utils_win.c - various utilities for MS Windows
    |  |  `-- xxhash.c - fast hashing algorithm
    |  |
    |  |-- args.c - command-line arguments parsing and processing
    |  |-- background.c - runs commands in background
    |  |-- bmarks.c - management of (named) bookmarks
    |  |-- bracket_notation.c - list of bracket notation entries
    |  |-- cmd_completion.c - handles command line completion
    |  |-- cmd_core.c - command line parsing
    |  |-- cmd_handlers.c - handlers for command line commands
    |  |-- compare.c - implementation of directory comparison
    |  |-- dir_stack.c - for :pushd and :popd commands
    |  |-- event_loop.c - event dispatching loop
    |  |-- filelist.c - fill/update list of files
    |  |-- filename_modifiers.c - expands filename modifiers
    |  |-- filetype.c - stores filetype information from vifmrc
    |  |-- filtering.c - managing file filters
    |  |-- flist_hist.c - file list history related code
    |  |-- flist_pos.c - most of file list scrolling/cursor positioning code
    |  |-- flist_sel.c - most of file list selection handling code
    |  |-- fops_common.c - shared functionality of high-level file operations
    |  |-- fops_cpmv.c - copying/moving/linking of files
    |  |-- fops_misc.c - most of high-level operations on files
    |  |-- fops_put.c - putting of files
    |  |-- fops_rename.c - renaming files in various ways
    |  |-- instance.c - manages generic state of the instance
    |  |-- ipc.c - handles communication across instances of vifm
    |  |-- macros.c - code of macros expansion
    |  |-- marks.c - stores information about marked directories
    |  |-- ops.c - most of operations performed on file system
    |  |-- opt_handlers.c - initialization of options and option change handlers
    |  |-- plugins.c - plugin management
    |  |-- registers.c - implementation of registers
    |  |-- running.c - code of handing file and commands running
    |  |-- search.c - code for / and ? commands of normal mode
    |  |-- signals.c - handlers for different signals
    |  |-- sort.c - sort function
    |  |-- status.c - definition of global status structure
    |  |-- tags.c - tags for :h completion
    |  |-- trash.c - code that handles list of files in trash
    |  |-- types.c - internal file type detection and conversions
    |  |-- undo.c - stores and handles the undo list
    |  |-- vcache.c - cache of viewers' output
    |  |-- version.c - git hash and other version information
    |  |-- viewcolumns_parser.c - contains code for parsing 'viewcolumns' option
    |  |-- vifm.c - contains main initialization/termination code
    |  `-- win_helper.c - needed for temporary rights elevation on Windows
    |
    |-- tests/ - test suites
    |
    |-- AUTHORS - list of code contributors
    |-- BUGS - some of known issues
    |-- ChangeLog - list of changes
    |-- FAQ - some common questions
    |-- HACKING.md - this file
    |-- INSTALL - building instructions
    |-- NEWS - like the ChangeLog, but in more human-readable format
    |-- README - readme used for distribution
    |-- README.md - readme to be displayed by code hosting services
    |-- THANKS - thanks to people that help to improve vifm
    `-- TODO - what still needs to be implemented
