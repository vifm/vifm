" Vim plugin for running vifm from vim.

" Maintainer: Ken Steen <ksteen@users.sourceforge.net>
" Last Change: 2001 November 29

" Maintainer: xaizek <xaizek@lavabit.com>
" Last Change: 2012 March 6

" vifm and vifm.vim can be found at http://vifm.sf.net

""""""""""""""""""""""""""""""""""""""""""""""""""""""""

if exists('loaded_vifm')
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
"		command -bar -nargs=* -complete=dir Insert_Your_Command_Name_Here :call s:StartVifm('edit', <f-args>)
"	endif

if !exists(':EditVifm')
	command -bar -nargs=* -complete=dir EditVifm :call s:StartVifm('edit', <f-args>)
endif
if !exists(':VsplitVifm')
	command -bar -nargs=* -complete=dir VsplitVifm :call s:StartVifm('vsplit', <f-args>)
endif
if !exists(':SplitVifm')
	command -bar -nargs=* -complete=dir SplitVifm :call s:StartVifm('split', <f-args>)
endif
if !exists(':DiffVifm')
	command -bar -nargs=* -complete=dir DiffVifm :call s:StartVifm('vert diffsplit', <f-args>)
endif
if !exists(':TabVifm')
	command -bar -nargs=* -complete=dir TabVifm :call s:StartVifm('tablast | tab drop', <f-args>)
endif

function! s:StartVifm(editcmd, ...)
	echohl WarningMsg | echo 'vifm executable wasn''t found' | echohl None
endfunction

if !exists('g:vifm_exec')
	let g:vifm_exec = 'vifm'
endif

if !exists('g:vifm_exec_args')
	let g:vifm_exec_args = ''
endif

if !exists('g:vifm_term')
	let g:vifm_term = 'xterm -e'
endif

if has('win32')
	if filereadable(g:vifm_exec) || filereadable(g:vifm_exec.'.exe')
		let s:vifm_home = fnamemodify(g:vifm_exec, ':p:h')
	else
		let s:vifm_home = $PATH
		let s:vifm_home = substitute(s:vifm_home, ';', ',', 'g').',.'
		let s:lst_str = globpath(s:vifm_home, g:vifm_exec, 1)
		let s:lst = split(s:lst_str, '\n')
		if empty(s:lst)
			let s:lst_str = globpath(s:vifm_home, g:vifm_exec.'.exe', 1)
			let s:lst = split(s:lst_str, '\n')
		endif
		if empty(s:lst)
			finish
		endif
		let s:vifm_home = s:lst[0]
		unlet s:lst_str
		unlet s:lst
	endif
	if !filereadable(s:vifm_home.'/vifmrc')
		unlet s:vifm_home
	endif
endif

if !exists('s:vifm_home')
	if exists('$HOME') && !isdirectory('$APPDATA/Vifm')
		let s:vifm_home = $HOME."/.vifm"
	elseif exists('$APPDATA')
		let s:vifm_home = $APPDATA."/Vifm"
	else
		finish
	endif
endif

function! s:StartVifm(editcmd, ...)
	if a:0 > 2
		echohl WarningMsg | echo 'Too many arguments' | echohl None
		return
	endif

	let ldir = (a:0 > 0) ? a:1 : expand('%:p:h')
	let ldir = s:PreparePath(ldir)
	let rdir = (a:0 > 1) ? a:2 : ''
	let rdir = s:PreparePath(rdir)

	" Gvim cannot handle ncurses so run vifm in a terminal.
	if has('gui_running')
		execute 'silent !' g:vifm_term g:vifm_exec '-f' g:vifm_exec_args ldir
		      \ rdir
	else
		execute 'silent !' g:vifm_exec '-f' g:vifm_exec_args ldir rdir
	endif

	redraw!

	" The selected files are written and read from a file instead of using
	" vim's clientserver so that it will work in the console without a X server
	" running.

	let vimfiles = fnamemodify(s:vifm_home.'/vimfiles', ':p')
	if !file_readable(vimfiles)
		echohl WarningMsg | echo 'vimfiles file not found' | echohl None
		return
	endif

	let flist = readfile(vimfiles)

	call map(flist, 'fnameescape(v:val)')

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

	" Don't split if current window is empty
	let firstfile = flist[0]
	if expand('%') == '' && a:editcmd =~ '^v\?split$'
		execute 'edit' fnamemodify(flist[0], ':.')
		let flist = flist[1:-1]
	endif

	for file in flist
		execute a:editcmd fnamemodify(file, ':.')
	endfor
	" Go to first file
	execute 'drop' firstfile
endfunction

function! s:PreparePath(path)
	let path = substitute(a:path, '\', '/', 'g')
	if has('win32')
		if len(path) != 0
			let path = '"'.path.'"'
		endif
	else
		let path = escape(fnameescape(path), '()')
	endif
	return path
endfunction

" vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab :
