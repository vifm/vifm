## Git

This plugin provides utilities related to Git.

### `:Gclone` command

**Syntax:** `:Gclone URL [dir-name]`

Clones repository, optionally deriving target directory name from the URL.  On
success, enters the newly cloned worktree.  On failure, shows the error in a
dialog.

**Examples:**

 * Clone into directory called `vifm`:
   ```
   :Gclone https://github.com/vifm/vifm.git
   ```

 * Clone into directory called `vifm-dev`:
   ```
   :Gclone https://github.com/vifm/vifm.git vifm-dev
   ```

**Parameters:**

 1. URL to clone from (required).
 2. Destination (optional).  Derived from the last component of the URL removing
    `.git` suffix.

### `:Gmap` command

**Syntax:** `:Gmap pattern replacement`

Adds a simple replacement rule for Git statuses.  Each new rule is appended to
the list.  The rules are processed in the order of their definition, therefore
earlier ones have higher priority and rules with wildcards should follow more
specific ones.

**Examples:**

 * `:Gmap D* x` marks files that are deleted in index (`D ` and `DU`) with `x`.
 * `:Gmap ?? —` replaces `??` with `—`.

**Parameters:**

 1. Pattern (required).  A two-character sequence where `*` is used as a
    wildcard that matches any character.
 2. Replacement (required).

### `:Gopt` command

**Syntax:** `:Gmap option...`

Configures behaviour by processing one or more options.

Default state (see **Parameters** section below for explanations):
 - `unchanged`

**Examples:**

 * `:Gopt nounchanged` disables marking unchanged files in any way
 * `:Gopt unchanged` enabling marking unchanged files as `GG`

**Parameters:**

 - `unchanged` -- mark unchanged files as `GG`
 - `nounchanged` -- don't mark unchanged files (`  `)

### `GitStatus` view column

When inside a Git repository, displays status of files and directories in a way
similar to how `git status --short` does it.

**Possible column values for files:**

| Value | Meaning
| ----- | -------
| `  `  | Ignored or unchanged (configurable, see `:Gopt`).
| ` M`  | Modified in worktree.
| `MM`  | Modified in index and in worktree.
| `AM`  | Added in index, modified in worktree.
| `RM`  | Renamed in index, modified in worktree.
| `M `  | Modified in index.
| `A `  | Added in Index.
| `R `  | Renamed in index.
| `D `  | Deleted in index.
| ` D`  | Deleted in worktree.
| `MD`  | Modified in index, deleted in worktree.
| `AD`  | Added in index, deleted in worktree.
| `RD`  | Renamed in index, deleted in worktree.
| `UD`  | Updated here, but deleted in merged changes.
| `DU`  | Deleted here, but updated in merged changes.
| `AA`  | Added here and in merged changes.
| `UU`  | Modified here and in merged changes.
| `GG`  | Unchanged (configurable, see `:Gopt`).
| `??`  | Untracked.

**Possible column values for directories:**

 * Any of the values for files.
 * If a subtree has multiple files modified, each character of the status is
   merged like this:
   * space + space = space
   * space + non-space = non-space
   * non-space + identical non-space = non-space
   * non-space + different non-space = 'X'
   * 'X' + anything = 'X'

**Example:**

 * `:set viewcolumns=-3{GitStatus},*{name}..,6{}.`

**Issues:**

 * Git status for a submodule isn't fetched immediately after entering it.

**TODO:**

 * Could make more behaviour optional via `:Gopt`:
   - asynchronous/synchronous: as a workaround for large repositories
   - marking ignored files
   - discovering git repositories and showing their statuses
 * Reuse old cache for entire subtree while it's being updated (only a specific
   path gets to reuse it right now).
 * Make cache update faster if directory modification is detected.
