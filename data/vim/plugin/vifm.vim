" Vim plugin for running vifm from vim.

" Maintainer: Ken Steen <ksteen@users.sourceforge.net>
" Last Change: 2001 November 29

" Maintainer: xaizek <xaizek@openmailbox.org>
" Last Change: 2014 November 05

" vifm and vifm.vim can be found at http://vifm.info/

""""""""""""""""""""""""""""""""""""""""""""""""""""""""

if exists('loaded_vifm')
	finish
endif
let loaded_vifm = 1

" Remember path to the script to know where to look for vifm documentation.
let s:script_path = expand('<sfile>')

" Setup commands to run vifm.

" :EditVifm - open file or files in current buffer.
" :SplitVifm - split buffer and open file or files.
" :VsplitVifm - vertically split buffer and open file or files.
" :DiffVifm - load file for :vert diffsplit.
" :TabVifm - load file or files in tabs.

command! -bar -nargs=* -complete=dir EditVifm :call s:StartVifm('edit', <f-args>)
command! -bar -nargs=* -complete=dir VsplitVifm :call s:StartVifm('vsplit', <f-args>)
command! -bar -nargs=* -complete=dir SplitVifm :call s:StartVifm('split', <f-args>)
command! -bar -nargs=* -complete=dir DiffVifm :call s:StartVifm('vert diffsplit', <f-args>)
command! -bar -nargs=* -complete=dir TabVifm :call s:StartVifm('tablast | tab drop', <f-args>)

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
	if has('win32')
		if filereadable('C:\Windows\system32\cmd.exe')
			let g:vifm_term = 'C:\Windows\system32\cmd.exe /C'
		else
			" If don't find use the integrate shell it work too
			let g:vifm_term = ''
		endif
	else
		let g:vifm_term = 'xterm -e'
	endif
endif

if !exists('g:vifm_home') &&  has('win32')
	if filereadable(g:vifm_exec) || filereadable(g:vifm_exec.'.exe')
		let g:vifm_home = fnamemodify(g:vifm_exec, ':p:h')
	else
		let g:vifm_home = $PATH
		let g:vifm_home = substitute(g:vifm_home, ';', ',', 'g').',.'
		let s:lst_str = globpath(g:vifm_home, g:vifm_exec, 1)
		let s:lst = split(s:lst_str, '\n')
		if empty(s:lst)
			let s:lst_str = globpath(g:vifm_home, g:vifm_exec.'.exe', 1)
			let s:lst = split(s:lst_str, '\n')
		endif
		if empty(s:lst)
			finish
		endif
		let g:vifm_home = s:lst[0]
		unlet s:lst_str
		unlet s:lst
	endif
	if !filereadable(g:vifm_home.'/vifmrc')
		unlet g:vifm_home
	endif
endif

if !exists('g:vifm_home')
	if exists('$HOME') && isdirectory($HOME .'/.vifm/')
		let g:vifm_home = $HOME."/.vifm"
	elseif exists('$APPDATA') && isdirectory($APPDATA.'/Vifm/')
		let g:vifm_home = $APPDATA."/Vifm"
	else
		echohl WarningMsg | echo 'Impossible to find your vifm configuration directory. Launch vifm one time and try again.' | echohl None
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

	if v:shell_error != 0
		echohl WarningMsg | echo 'Got non-zero code from vifm' | echohl None
		return
	endif

	" The selected files are written and read from a file instead of using
	" vim's clientserver so that it will work in the console without a X server
	" running.

	let vimfiles = fnamemodify(g:vifm_home.'/vimfiles', ':p')
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

" K {{{1

" Mostly stolen from vim-scriptease, created by Tim Pope <http://tpo.pe/>

function! vifm#synnames(...) abort
	if a:0
		let [line, col] = [a:1, a:2]
	else
		let [line, col] = [line('.'), col('.')]
	endif
	return reverse(map(synstack(line, col), 'synIDattr(v:val,"name")'))
endfunction

let g:vifm_help_mapping = get(g:, 'vifm_help_mapping', 'K')

augroup VifmHelpAutoCmds
	autocmd!
	execute "autocmd FileType vifm,vifm-cmdedit nnoremap <silent><buffer>"
	      \ g:vifm_help_mapping ":execute <SID>DisplayVifmHelp()<CR>"
augroup END

" Modifies 'runtimepath' to include directory with vifm documentation and runs
" help.  Result should be processed with :execute to do not print stacktrace
" on exception.
function! s:DisplayVifmHelp()
	let runtimepath = &runtimepath
	let vimdoc = substitute(s:script_path, '[/\\]plugin[/\\].*', '', '')
	execute 'set runtimepath+='.vimdoc.'/../vim-doc'

	try
		execute 'help '.s:GetVifmHelpTopic()
	catch E149
		let msg = substitute(v:exception, '[^:]\+:', '', '')
		return 'echoerr "'.escape(msg, '\"').'"'
	finally
		let &runtimepath = runtimepath
	endtry
	return ''
endfunction

function! s:GetVifmHelpTopic()
	let col = col('.') - 1
	while col && getline('.')[col] =~# '\k'
		let col -= 1
	endwhile
	let pre = col == 0 ? '' : getline('.')[0 : col]
	let syn = get(vifm#synnames(), 0, '')
	let cword = expand('<cword>')
	if syn ==# 'vifmBuiltinFunction'
		let topic = cword.'()'
	elseif syn ==# 'vifmOption'
		let topic = "'".substitute(cword, '^\(no\|inv\)', '', '')."'"
	elseif syn ==# 'vifmCommand' || pre =~# ':$'
		let topic = ':'.cword
	elseif syn ==# 'vifmNotation'
		let topic = 'mappings'
	else
		let topic = '*'.cword
	endif
	return 'vifm-'.topic
endfunction

" }}}1

" vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab :
