" Vim plugin for running vifm from vim.

" Maintainer: Ken Steen <ksteen@users.sourceforge.net>
" Last Change: 2001 November 29

" Maintainer: xaizek <xaizek@openmailbox.org>
" Last Change: 2017 May 30

" vifm and vifm.vim can be found at https://vifm.info/

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

" Check whether :drop command is available
try
	drop
catch E471
	let s:has_drop = 1
catch E319
	let s:has_drop = 0
endtry

let s:tab_drop_cmd = (s:has_drop ? 'tablast | tab drop' : 'tabedit')

command! -bar -nargs=* -complete=dir EditVifm :call s:StartVifm('edit', <f-args>)
command! -bar -nargs=* -complete=dir VsplitVifm :call s:StartVifm('vsplit', <f-args>)
command! -bar -nargs=* -complete=dir SplitVifm :call s:StartVifm('split', <f-args>)
command! -bar -nargs=* -complete=dir DiffVifm :call s:StartVifm('vert diffsplit', <f-args>)
command! -bar -nargs=* -complete=dir TabVifm :call s:StartVifm(s:tab_drop_cmd, <f-args>)

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

function! s:StartVifm(editcmd, ...)
	if a:0 > 2
		echohl WarningMsg | echo 'Too many arguments' | echohl None
		return
	endif

	let ldir = (a:0 > 0) ? a:1 : expand('%:p:h')
	let ldir = s:PreparePath(ldir)
	let rdir = (a:0 > 1) ? a:2 : ''
	let rdir = s:PreparePath(rdir)

	let listf = tempname()
	let typef = tempname()

	" XXX: this is horrible, but had to do this to work around selection
	"      clearing after each command-line command (:let in this case)
	let edit = ' | execute ''cnoremap j <cr>'' | normal gs:editj'

	let pickargs = [
	    \ '--choose-files', listf,
	    \ '--on-choose',
	    \ has('win32')
	    \ ? 'echo \"%%VIFM_OPEN_TYPE%%\">' . typef : 'echo $VIFM_OPEN_TYPE >' . typef,
	    \ '+command EditVim   :let $VIFM_OPEN_TYPE=''edit''' . edit,
	    \ '+command VsplitVim :let $VIFM_OPEN_TYPE=''vsplit''' . edit,
	    \ '+command SplitVim  :let $VIFM_OPEN_TYPE=''split''' . edit,
	    \ '+command DiffVim   :let $VIFM_OPEN_TYPE=''vert diffsplit''' . edit,
	    \ '+command TabVim    :let $VIFM_OPEN_TYPE='''.s:tab_drop_cmd."'" . edit]
	call map(pickargs, 'shellescape(v:val, 1)')
	let pickargsstr = join(pickargs, ' ')

	if !has('nvim')
		" Gvim cannot handle ncurses so run vifm in a terminal.
		if has('gui_running')
			execute 'silent !' g:vifm_term g:vifm_exec g:vifm_exec_args ldir rdir
			      \ pickargsstr
		else
			execute 'silent !' g:vifm_exec g:vifm_exec_args ldir rdir pickargsstr
		endif

		" Execution of external command might have left Vim's window cleared, force
		" redraw before doing anything else.
		redraw!

		call s:HandleRunResults(v:shell_error, listf, typef, a:editcmd)
	else
		" Work around handicapped neovim...
		let callback = { 'listf': listf, 'typef' : typef, 'editcmd' : a:editcmd }
		function! callback.on_exit(id, code)
			buffer #
			silent! bdelete! #
			call s:HandleRunResults(a:code, self.listf, self.typef, self.editcmd)
		endfunction
		enew
		call termopen(g:vifm_exec . ' ' . g:vifm_exec_args . ' ' . ldir . ' ' . rdir
		             \. ' ' . pickargsstr, callback)
		execute 'keepalt file' escape('vifm: '.a:editcmd, ' |')
		startinsert
	endif
endfunction

function! s:HandleRunResults(exitcode, listf, typef, editcmd)
	if a:exitcode != 0
		echohl WarningMsg
		echo 'Got non-zero code from vifm: ' . a:exitcode
		echohl None
		call delete(a:listf)
		call delete(a:typef)
		return
	endif

	" The selected files are written and read from a file instead of using
	" vim's clientserver so that it will work in the console without a X server
	" running.

	if !file_readable(a:listf)
		echohl WarningMsg | echo 'Failed to read list of files' | echohl None
		call delete(a:listf)
		call delete(a:typef)
		return
	endif

	let flist = readfile(a:listf)
	call delete(a:listf)

	let opentype = file_readable(a:typef) ? readfile(a:typef) : []
	call delete(a:typef)

	call map(flist, 'fnameescape(v:val)')

	" User exits vifm without selecting a file.
	if empty(flist)
		echohl WarningMsg | echo 'No file selected' | echohl None
		return
	endif

	if !empty(opentype) && !empty(opentype[0]) &&
		\ opentype[0] != '"%VIFM_OPEN_TYPE%"'
		let editcmd = has('win32') ? opentype[0][1:-2] : opentype[0]
	else
		let editcmd = a:editcmd
	endif

	" Don't split if current window is empty
	let firstfile = flist[0]
	if empty(expand('%')) && editcmd =~ '^v\?split$'
		execute 'edit' fnamemodify(flist[0], ':.')
		let flist = flist[1:-1]
	endif

	" We emulate :args to not leave unnamed buffer around after we open our
	" buffers.
	if editcmd == 'edit' && len(flist) > 1
		silent! %argdelete
	endif

	for file in flist
		execute editcmd fnamemodify(file, ':.')
		if editcmd == 'edit' && len(flist) > 1
			execute 'argadd' fnamemodify(file, ':.')
		endif
	endfor

	" Go to the first file working around possibility that :drop command is not
	" evailable, if possible
	if editcmd == 'edit'
		execute 'buffer' fnamemodify(firstfile, ':.')
	elseif s:has_drop
		" Mind that drop replaces arglist, so don't use it with :edit.
		execute 'drop' firstfile
	endif
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
