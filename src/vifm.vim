" Vim plugin for running vifm from vim.

" Maintainer: Ken Steen <ksteen@users.sourceforge.net>
" Last Change: 2001 November 29

" Maintainer: xaizek <xaizek@gmail.com>
" Last Change: 2011 August 2

" vifm and vifm.vim can be found at http://vifm.sf.net

""""""""""""""""""""""""""""""""""""""""""""""""""""""""

if exists("loaded_vifm")
	finish
endif
let loaded_vifm = 1

" Setup commands to run vifm.

" :EditVifm - open file or files in current buffer.
" :SplitVifm - split buffer and open file or files.
" :VsplitVifm - vertically split buffer and open file or files.
" :DiffVifm - load file for :vert diffsplit.
" :TabVifm - load file or files in tabs.

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
	command TabVifm :call s:StartVifm('tablast | tab drop')
endif

function! s:StartVifm(editcmd)
	" Gvim cannot handle ncurses so run vifm in an xterm.
	let dir = fnameescape(expand("%:p:h"))
	if has('gui_running')
		execute "silent !xterm -e vifm -f" dir
	else
		execute "silent !vifm -f" dir
	endif

	redraw!

	" The selected files are written and read from a file instead of using
	" vim's clientserver so that it will work in the console without a X server
	" running.
	let flist = readfile(fnamemodify('~/.vifm/vimfiles', ":p"))

	call map(flist, 'fnameescape( v:val )')

	" User exits vifm without selecting a file.
	if empty(flist)
		echohl WarningMsg | echo 'No file selected' | echohl None
		return
	endif

	if a:editcmd == 'edit'
		call map(flist, 'fnamemodify(v:val, ":.")')
		execute 'args' join(flist)
		return
	endif

	" Several files to open
	let firstfile = fnamemodify(flist[0], ':.')
	" Don't split if current window is empty
	if expand('%') == '' && a:editcmd =~ '^v\?split$'
		execute 'edit' firstfile
		let flist = flist[1:-1]
		" for file in [] does not seem to work, so we need to return
		if empty(flist)
			return
		endif
	endif
	for file in flist
		let file = fnamemodify(file, ':.')
		execute a:editcmd file
	endfor
	" Go to first file
	execute 'drop' firstfile
endfunction
