## Using Vifm as Helix's file-manager

### Setup

1. Copy this plugin to your Vifm's plugin directory

2. Add these three scripts to your PATH:

<details>
  <summary>tmux-ide</summary>

```bash
#!/bin/bash

file="$1"
if [ -d "$file" ]; then
  cd "$file"
fi

session_id="$(tmux new-session -dP)"
tmux rename-window -t "$session_id" "editor"
tmux send-keys -t "$session_id" "exec $EDITOR '$file'" Enter
exec tmux attach -t "$session_id"
```
</details>


<details>
  <summary>tmux-toggle</summary>

```bash
#!/bin/bash

main() {
  local tag mode cmd
  if [ $# -gt 1 ]; then
    mode="$1"
    tag="$2"
    cmd="$3"
    shift 3

    case "$mode" in
      pane) toggle_pane "$tag" "$cmd" "$@";;
      window) toggle_window "$tag" "$cmd" "$@";;
    esac
  fi
}

toggle_window() {
  local tag cmd target current_window
  tag="$1"
  cmd="$2"
  target="$(tmux show -qv "@$tag")"
  current_window="$(tmux display -p '#S:#I')"

  if [ "$current_window" = "$target" ]; then
    tmux last-window
  else
    if [ -n "$target" ] && tmux lsw -F'#S:#I' | grep -q "$target"; then
      tmux select-window -t "$target"
    else
      if [ -z "$cmd" ]; then
        target="$(tmux new-window -PF'#S:#I')"
      else
        target="$(tmux new-window -PF'#S:#I' -- sh -c "$cmd")"
      fi
      tmux set "@$tag" "$target"
    fi
  fi
}

toggle_pane() {
  local tag cmd size target current_window
  tag="$1"
  cmd="$2"
  size="$3"
  target="$(tmux show -qv "@$tag")"
  current_window="$(tmux display -p '#S:#I')"

  if [ "$current_window" = "${target%.*}" ]; then
    target="$(tmux break-pane -dPs "$target")"
  else
    if [ -n "$target" ]; then
      tmux join-pane -bhl "$size" -s "$target" -t "$current_window.0"
      target="$current_window.0"
    else
      if [ -z "$cmd" ]; then
        target="$(tmux split -bhl "$size" -PF'#S:#I.#P')"
      else
        target="$(tmux split -bhl "$size" -PF'#S:#I.#P' -- sh -c "$cmd")"
      fi
      tmux select-pane -t "$target" -P 'bg=#090B10'
    fi
  fi
  tmux set "@$tag" "$target"
}

main "$@"
```
</details>


<details>
  <summary>tmux-toggle-pane-vifm</summary>

```bash
#!/bin/bash

tmux-toggle pane fm 'vifm -c "set vicmd='\''#editor#run helix-tmux'\''" "$(pwd)"' 25%
```
</details>

3. Configure Tmux

<details>
  <summary>tmux.conf</summary>

```shell
bind -n 'c-b' run-shell 'tmux-toggle-pane-vifm'
bind -n 'c-t' run-shell 'tmux-toggle window term'

# Keybinding to open an editor window in a new tmux session
bind -n 'c-e' new-window \; rename-window editor \; send-keys -t :editor.0 'exec $EDITOR' Enter

# Theming
set -g default-terminal "tmux-256color"
set -as terminal-overrides ",alacritty*:Tc"
set -g status off
set -g pane-border-status off
set -g pane-border-indicators off
set -g pane-active-border-style "fg=#0F111A"
set -g pane-border-lines heavy
```
</details>

4. Configure Vifm:

<details>
  <summary>vifmrc</summary>

```vim
" Hides information when there is not enough space. Customize it however you want
if &columns > 160
    vsplit | view!
    set viewcolumns=-{name}..,7{size},12{perms},7{uname}..,-7{gname}..
    set statusline="%1* %t %2* %3*%{filetype('.')} %T %= %a / %c | %d %2*%1* %E "
elseif &columns > 81
    only
    set viewcolumns=-{name}..,7{size},12{perms},7{uname}..,-7{gname}..
    set statusline="%1* %t %2* %3*%{filetype('.')} %T %N%= %a / %c | %d %2*%1* %E "
else
    only
    set viewcolumns=-{name}..,
    set statusline=" "
    set rulerformat=" "
endif

" Toggles the preview window
command toggle :
\|  if layoutis('only')
\|    restart
\|  else
\|    view
\|    only
\|  endif

" Changes the current directory to the fuzzy finded file's directory and then opens it
command! FZFFindFilesOnly : set noquickview
\| let $FZF_PICK = term('find -type f | fzf 2>/dev/tty')
\| if $FZF_PICK != ''
\|   execute 'goto' fnameescape($FZF_PICK)
\|   execute 'edit' fnameescape(system('basename "$FZF_PICK"'))
\| endif

" In case you want to fuzzy find both directories and regular files
command! FZFFind : set noquickview
\| let $FZF_PICK = term('find | fzf 2>/dev/tty')
\| if $FZF_PICK != ''
\|   let $IS_DIR = system('[ -d "$FZF_PICK" ] && echo true || echo false')
\|   if $IS_DIR == 'true'
\|     execute 'cd' fnameescape($FZF_PICK)
\|   else
\|     execute 'goto' fnameescape($FZF_PICK)
\|     execute 'edit' fnameescape(system('basename "$FZF_PICK"'))
\|   endif
\| endif

" Good to have keybindings
nnoremap <c-f> :FZFFindFilesOnly<cr>
nnoremap T :tree depth=2<cr>
nnoremap B :toggle<cr>
nnoremap E :!nohup $TERMINAL -e tmux-ide %f &>/dev/null & disown<cr>
```
</details>

### Usage
Depending on your preference:
- Run `tmux` inside a terminal, then press `Ctrl-e` to create an editor window.
- Run `tmux-ide [optional file or directory path]` inside a terminal. It automatically creates an editor window.
- Inside Vifm, select a file or project directory, then press `Shift-e` to open a terminal that will execute `tmux-ide <the selected file or directory path>`

After that, you can press `Ctrl-b` to toggle the file manager or press `Ctrl-t` to toggle a terminal window.
