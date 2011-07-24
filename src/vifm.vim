" Vim plugin for running vifm from vim.

" Maintainer: Ken Steen <ksteen@users.sourceforge.net>
" Last Change: 2001 November 29

" Maintainer: xaizek <xaizek@gmail.com>
" Last Change: 2011 July 17

" vifm and vifm.vim can be found at http://vifm.sf.net

""""""""""""""""""""""""""""""""""""""""""""""""""""""""

if exists("loaded_vifm")
	finish
endif
let loaded_vifm = 1

" Setup commands to run vifm.

" :EditVifm - open file in current buffer.
" :SplitVifm - split buffer and open file.
" :VsplitVifm - vertically split buffer and open file.
" :DiffVifm - load file for :vert diffsplit.

" If the commands clash with your other user commands you can change the
" command name.

"	if !exists(':Insert_Your_Command_Name_Here')
"		command Insert_Your_Command_Name_Here :call s:StartVifm('edit')
"	endif

if !exists(':EditVifm')
	command EditVifm :call s:StartVifm('edit')
endif
if !exists(':VsplitVifm')
	command VsplitVifm :call s:StartVifm('vsplit')
endif
if !exists(':SplitVifm')
	command SplitVifm :call s:StartVifm('split')
endif
if !exists(':DiffVifm')
	command DiffVifm :call s:StartVifm('vert diffsplit')
endif
if !exists(":TabVifm")
  command TabVifm :call s:StartVifm('tab drop')
endif

function! s:StartVifm(editcmd)
	" Gvim cannot handle ncurses so run vifm in an xterm.
	if has('gui_running')
		silent !cd "%:p:h" && xterm -e vifm -f
	else
		silent !cd "%:p:h" && vifm -f
	endif

	redraw!

	" The selected files are written and read from a file instead of using
	" vim's clientserver so that it will work in the console without a X server
	" running.
	let s:r = system('cat ~/.vifm/vimfiles')

	" User exits vifm without selecting a file.
	if s:r =~ 'NULL'
		echohl WarningMsg | echo 'No file selected' | echohl None
		return
	endif

	let s:flist = split(s:r, "\n")

	if len(s:flist) == 1
		silent execute a:editcmd fnameescape(s:flist[0])
		return
	endif

	let s:flist = map(s:flist, 'fnameescape(v:val)')
	if a:editcmd == 'edit'
		silent execute 'args' join(s:flist)
    elseif a:editcmd == 'tab drop'
		silent execute 'tab drop' join(s:flist)
	else
		for s:file in s:flist
			silent execute a:editcmd s:file
		endfor
	endif
endfunction
