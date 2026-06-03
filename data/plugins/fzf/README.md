# fzf

fzf integration for vifm implemented as a Lua plugin.

The plugin registers five commands:

```vim
:FzfCommand [query]
:FzfFind [query]
:FzfFindStashed [query]
:FzfGrep [pattern]
:FzfLiveGrep [query]
```

The plugin currently targets POSIX systems (`/bin/sh`, `/dev/tty`, `find`,
`grep`/`rg`).  On Windows it is unlikely to work as-is.

## Installation

Put this directory under one of vifm plugin directories, for example:

```sh
~/.config/vifm/plugins/fzf/init.lua
```

The plugin directory must contain `init.lua` directly.  If you use
`--plugins-dir`, point it at the parent directory that contains `fzf`, not at
the `fzf` directory itself:

```sh
vifm --plugins-dir ~/.config/vifm/plugins
```

From the source tree this can be tested with:

```sh
src/vifm --plugins-dir data/plugins
```

Restart vifm after copying the plugin if plugins were already loaded.

## Requirements

- `fzf` is required for all commands.
- `rg` is used by `:FzfFind` when available.
- `find` is used by `:FzfFind` as a fallback when `rg` is not available.
- `rg` is used by `:FzfGrep` when available.
- `grep` is used by `:FzfGrep` as a fallback when `rg` is not available.

## Commands

### `:FzfCommand [query]`

Opens an fzf-backed command palette.  It lists user-defined commands, builtin
commands and described normal-mode keys.  Selecting a command executes it.  If
the selected command requires arguments, vifm opens the command line with the
command name prefilled.

The optional `[query]` is passed to fzf as the initial query.

### `:FzfFind [query]`

Searches files under the current view directory and lets fzf pick a file.
`rg --files --hidden --no-ignore --one-file-system` is used when available,
otherwise the command falls back to `find -xdev`.  Mounted directories below the
search root are skipped; when they can be detected, fzf displays them in its
header.  After selection, vifm navigates to the picked path.

The optional `[query]` is passed to fzf as the initial query.

### `:FzfFindStashed [query]`

Same as `:FzfFind`, but also stores matching paths in a custom menu so the
result list can be reopened through vifm's menu history/navigation.

The optional `[query]` is passed to fzf as the initial query.

### `:FzfGrep [pattern]`

Searches file contents under the current view directory and shows matches in
fzf.  Pressing Enter on a match opens the file at the matching line using
`'vicmd'`.

The selected match is opened in `'vicmd'` which is expanded by the shell.  When
`'vicmd'` contains additional arguments the whole string is used verbatim, so
make sure it works as a `sh` command line.

If `[pattern]` is omitted, `.` is used.

## Example mappings

```vim
nnoremap ,c :FzfCommand<cr>
nnoremap ,f :FzfFind<cr>
nnoremap ,F :FzfFindStashed<cr>
nnoremap ,g :FzfGrep
nnoremap ,G :FzfLiveGrep<cr>
```
