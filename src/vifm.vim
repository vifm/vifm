" Vim plugin for running vifm from vim.

" Maintainer: Ken Steen <ksteen@users.sourceforge.net>
" Last Change: 2001 November 29

" vifm and vifm.vim can be found at http://vifm.sf.net

""""""""""""""""""""""""""""""""""""""""""""""""""""""""


if exists("loaded_vifm")
	finish
endif
let loaded_vifm=1

"Setup commands to run vifm.

":EditVifm - open file in current buffer.
":SplitVifm - split buffer and open file.
":VsplitVifm - vertically split buffer and open file.
":DiffVifm - load file for :vert diffsplit.

" If the commands clash with your other user commands you can change the 
"  	command name. 

"   	if !exists(":Insert_Your_Command_Name_Here")
"					command Insert_Your_Command_Name_Here :call s:StartVifm("edit ")

if !exists(':EditVifm') 
	command EditVifm :call s:StartVifm("edit ")
endif
if !exists(':VsplitVifm')
	command VsplitVifm :call s:StartVifm("vs ")
endif
if !exists(':SplitVifm')
	command SplitVifm :call s:StartVifm("split ")
endif
if !exists(':DiffVifm')
	command DiffVifm :call s:StartVifm("diff ")
endif

function! s:StartVifm(editcmd)

"Gvim cannot handle ncurses so run vifm in an xterm.
	if has("gui_running")
		silent !xterm -e vifm -f 
	else
		silent !vifm -f 
	endif

	redraw!

	"The selected files are written and read from a file instead of using
	"vim's clientserver so that it will work in the console without a X server
	"running.
	let s:r =system("cat ~/.vifm/vimfiles")

	"User exits vifm without selecting a file."
	if s:r =~"NULL"
		echohl WarningMsg | echo "No file selected" | echohl None
	else
		if a:editcmd =~"edit " 
			let s:cmd ="edit " 
			exe s:cmd . s:r
		elseif a:editcmd =~"vs" 
			let s:cmd ="vsplit " 
			exe s:cmd . s:r
		elseif a:editcmd =~"split" 
			let s:cmd ="split "
			exe s:cmd . s:r
		elseif a:editcmd =~"diff" 
			let s:cmd ="vert diffsplit "
			exe s:cmd . s:r
		endif
	endif

endfunction



