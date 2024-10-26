" Maintainer: xaizek <xaizek@posteo.net>
" Last Change: 2024 October 25

" Author: Ken Steen <ksteen@users.sourceforge.net>
" Last Change: 2001 November 29

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
" :PeditVifm - open file in preview window.
" :SplitVifm - split buffer and open file or files.
" :VsplitVifm - vertically split buffer and open file or files.
" :DiffVifm - load file for :vert diffsplit.
" :TabVifm - load file or files in tabs.

" Check whether :drop command is available.  Do not use exist(':drop'), it's
" deceptive.
let s:has_drop = 0
try
	drop
catch /E471:/ " argument required
	let s:has_drop = 1
catch /E319:/ " command is not available
catch /E492:/ " not an editor command
endtry

let s:tab_drop_cmd = (s:has_drop ? 'tablast | tab drop' : 'tabedit')

command! -bar -nargs=* -count -complete=dir Vifm
			\ :call s:StartVifm('<mods>', <count>, 'edit', <f-args>)
command! -bar -nargs=* -count -complete=dir EditVifm
			\ :call s:StartVifm('<mods>', <count>, 'edit', <f-args>)
command! -bar -nargs=* -count -complete=dir PeditVifm
			\ :call s:StartVifm('<mods>', <count>, 'pedit', <f-args>)
command! -bar -nargs=* -count -complete=dir VsplitVifm
			\ :call s:StartVifm('<mods>', <count>, 'vsplit', <f-args>)
command! -bar -nargs=* -count -complete=dir SplitVifm
			\ :call s:StartVifm('<mods>', <count>, 'split', <f-args>)
command! -bar -nargs=* -count -complete=dir DiffVifm
			\ :call s:StartVifm('<mods>', <count>, 'vert diffsplit', <f-args>)
command! -bar -nargs=* -count -complete=dir TabVifm
			\ :call s:StartVifm('<mods>', <count>, s:tab_drop_cmd, <f-args>)

command! -bar -nargs=* -complete=color VifmCs
			\ :call vifm#colorconv#convert(<f-args>)

function! s:StartVifm(mods, count, editcmd, ...) abort
	echoerr 'vifm executable wasn''t found'
endfunction

call vifm#globals#Init()

if !has('nvim') && exists('*term_start')
	function! VifmExitCb(data, job, code) abort
		let data = a:data
		if (bufnr('%') == bufnr('#') || !bufexists(0)) && !data.split
			enew
		else
			silent! buffer #
		endif
		silent! bdelete! #
		if data.split
			silent! close
		endif
		if has('job') && type(data.cwdjob) == v:t_job
			call job_stop(data.cwdjob)
		endif
		call s:HandleRunResults(a:code, data.listf, data.typef, data.editcmd,
		                      \ data.snapshot)
	endfunction
endif

function! s:DetermineTermEnv() abort
	if !has('gui_running')
		return (&term =~ 256 ? 'xterm-256color' : &term)
	endif

	if empty($TERM) || $TERM == 'dumb'
		return 'xterm-256color'
	endif

	return $TERM
endfunction

function! s:UniqueBufferName(name) abort
	let i = 2
	let name = a:name
	while bufexists(name)
		let name = a:name . ' (' . i . ')'
		let i = i + 1
	endwhile
	return name
endfunction

function! s:StartVifm(mods, count, editcmd, ...) abort
	if a:0 > 2
		echoerr 'Too many arguments'
		return
	endif

	let embed = has('nvim') || exists('*term_start') && g:vifm_embed_term

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
	    \ '+command PeditVim  :let $VIFM_OPEN_TYPE=''pedit''' . edit,
	    \ '+command TabVim    :let $VIFM_OPEN_TYPE='''.s:tab_drop_cmd."'" . edit]
	call map(pickargs, embed ? 'shellescape(v:val)' : 'shellescape(v:val, 1)')
	let pickargsstr = join(pickargs, ' ')

	let bufsnapshot = s:TakeBufferSnapshot()

	" Use embedded terminal if available.
	if embed
		let [cwdf, cwdjob] = s:StartCwdJob()

		if cwdf != ''
			let cwdargs = '-c "autocmd DirEnter * !pwd >> ' . shellescape(cwdf) . '"'
		else
			let cwdargs = ''
		endif

		let data = { 'listf' : listf, 'typef' : typef, 'editcmd' : a:editcmd,
					\ 'cwdjob' : cwdjob, 'split': get(g:, 'vifm_embed_split', 0),
					\ 'snapshot' : bufsnapshot }

		if !has('nvim')
			let env = { 'TERM' : s:DetermineTermEnv() }
			let options = { 'term_name' : 'vifm: '.a:editcmd, 'curwin' : 1,
			              \ 'exit_cb': funcref('VifmExitCb', [data]), 'env' : env }
		else
			function! data.on_exit(id, code, event) abort
				if (bufnr('%') == bufnr('#') || !bufexists(0)) && !self.split
					enew
				else
					silent! buffer #
				endif
				silent! bdelete! #
				if self.split
					silent! close
				endif
				if self.cwdjob != 0
					call jobstop(self.cwdjob)
				endif
				call s:HandleRunResults(a:code, self.listf, self.typef, self.editcmd,
				                      \ self.snapshot)
			endfunction
		endif

		if data.split
			exec a:mods . ' ' . (a:count ? a:count : '') . 'new'
		else
			enew
		endif

		let termcmd = g:vifm_exec.' '.g:vifm_exec_args.' '.cwdargs.' '.ldir.' '
					\ .rdir.' '.pickargsstr

		if !has('nvim')
			if has('win32')
				keepalt let buf = term_start(termcmd, options)
			else
				keepalt let buf = term_start(['/bin/sh', '-c', termcmd], options)
			endif
		else
			call termopen(termcmd, data)

			let oldbuf = bufname('%')
			let newbuf = s:UniqueBufferName('vifm: '.a:editcmd)
			execute 'keepalt file' escape(newbuf, ' |')
			" This is for Neovim, which uses these options even in terminal mode
			setlocal nonumber norelativenumber nospell
			execute bufnr(oldbuf).'bwipeout'
			" Use execute to not break highlighting.
			execute 'startinsert'
		endif

		return
	else
		" Gvim cannot handle ncurses so run vifm in a terminal.
		if has('gui_running')
			execute 'silent noautocmd !'
			      \ g:vifm_term g:vifm_exec g:vifm_exec_args ldir rdir pickargsstr
		else
			execute 'silent noautocmd !'
			      \ g:vifm_exec g:vifm_exec_args ldir rdir pickargsstr
		endif

		" Execution of external command might have left Vim's window cleared, force
		" redraw before doing anything else.
		redraw!

		call s:HandleRunResults(v:shell_error, listf, typef, a:editcmd, bufsnapshot)
	endif
endfunction

" Makes list of open buffers backed up by files.  Invoked before starting a Vifm
" instance.
function! s:TakeBufferSnapshot() abort
	let buffer_snapshot = []
	for buf in getbufinfo({ 'buflisted': 1 })
		if filereadable(buf.name)
			call add(buffer_snapshot, buf.bufnr)
		endif
	endfor
	return buffer_snapshot
endfunction

" Closes unchanged buffers snapshotted by TakeBufferSnapshot() which no longer
" correspond to any buffer.  Invoked after Vifm has closed (even with an error
" code, because file system could have been updated).
function! s:DropGoneBuffers(buffer_snapshot, excluded_bufnrs) abort
	let gone_buffers = []
	for bufnr in a:buffer_snapshot
		if bufexists(bufnr) && !get(a:excluded_bufnrs, bufnr)
			let info = getbufinfo(bufnr)[0]
			" Do not close a changed buffer even if its file is gone.
			if !info.changed && !filereadable(info.name)
				call add(gone_buffers, bufnr)
			endif
		endif
	endfor
	if !empty(gone_buffers)
		execute 'silent! bwipeout ' join(gone_buffers, ' ')
	endif
endfunction

" Opens files after exiting Vifm.  Returns a dict of opened buffer names.
function! s:OpenFiles(editcmd, flist, opentype) abort
	let opened_bufnrs = {}

	" User exits vifm without selecting a file.
	if empty(a:flist)
		echohl WarningMsg | echo 'No file selected' | echohl None
		return opened_bufnrs
	endif

	let flist = a:flist
	call map(flist, 'resolve(fnamemodify(v:val, ":."))')
	let firstfile = flist[0]

	if !empty(a:opentype) && !empty(a:opentype[0]) &&
		\ a:opentype[0] != '"%VIFM_OPEN_TYPE%"'
		let editcmd = has('win32') ? a:opentype[0][1:-2] : a:opentype[0]
	else
		let editcmd = a:editcmd
	endif

	" Don't split if current window is empty
	if empty(expand('%')) && editcmd =~ '^v\?split$'
		execute 'edit' fnameescape(flist[0])
		let opened_bufnrs[bufnr(flist[0])] = 1
		let flist = flist[1:-1]
		if len(flist) == 0
			return opened_bufnrs
		endif
	endif

	" We emulate :args to not leave unnamed buffer around after we open our
	" buffers.
	if editcmd == 'edit' && len(flist) > 1
		silent! %argdelete
	endif

	" Doesn't make sense to run :pedit multiple times in a row.
	if editcmd == 'pedit' && len(flist) > 1
		let flist = [ flist[0] ]
	endif

	for file in flist
		if s:has_drop && editcmd == s:tab_drop_cmd && empty(glob(file))
			try
				let [wildignore, &wildignore] = [&wildignore, '']
				execute editcmd fnameescape(file)
			finally
				" If some auto-command has changed 'wildignore', we'll overwrite it
				" here...
				let &wildignore = wildignore
			endtry
		else
			execute editcmd fnameescape(file)
		endif

		let opened_bufnrs[bufnr(file)] = 1
		if editcmd == 'edit' && len(flist) > 1
			execute 'argadd' fnameescape(file)
		endif
	endfor

	" When we open a single file, there is no need to navigate to its window,
	" because we're already there
	if len(flist) == 1
		return opened_bufnrs
	endif

	" Go to the first file working around possibility that :drop command is not
	" evailable, if possible
	if editcmd == 'edit' || !s:has_drop
		execute 'buffer' fnameescape(firstfile)
	elseif s:has_drop
		" Mind that drop replaces arglist, so don't use it with :edit.
		execute 'drop' fnameescape(firstfile)
	endif

	return opened_bufnrs
endfunction

function! s:StartCwdJob() abort
	if get(g:, 'vifm_embed_cwd', 0) && (has('job') || has('nvim'))
		let cwdf = tempname()
		silent! exec '!mkfifo '. cwdf

		let cwdcmd = ['/bin/sh', '-c',
					\ 'while true; do cat ' . shellescape(cwdf) . '; done']

		if !has('nvim')
			let cwdopts = { 'out_cb': 'VifmCwdCb' }

			function! VifmCwdCb(channel, data) abort
				call s:HandleCwdOut(a:data)
			endfunction

			let cwdjob = job_start(cwdcmd, cwdopts)
		else
			let cwdopts = {}

			function! cwdopts.on_stdout(id, data, event) abort
				if a:data[0] ==# ''
					return
				endif
				call s:HandleCwdOut(a:data[0])
			endfunction

			let cwdjob = jobstart(cwdcmd, cwdopts)
		endif

		return [cwdf, cwdjob]
	endif
	return ['', 0]
endfunction

function! s:HandleCwdOut(data) abort
	exec 'cd ' . fnameescape(a:data)
endfunction

function! s:HandleRunResults(exitcode, listf, typef, editcmd, bufsnapshot) abort
	let err = 0

	if a:exitcode != 0
		echoerr 'Got non-zero code from vifm: ' . a:exitcode
		let err = 1
	endif

	" The selected files are written and read from a file instead of using
	" vim's clientserver so that it will work in the console without a X server
	" running.

	if !err && !file_readable(a:listf)
		echoerr 'Failed to read list of files'
		let err = 1
	endif

	let opened_bufnrs = {}

	if !err
		let flist = readfile(a:listf)
		let opentype = file_readable(a:typef) ? readfile(a:typef) : []

		call delete(a:listf)
		call delete(a:typef)

		let opened_bufnrs = s:OpenFiles(a:editcmd, flist, opentype)
	else
		call delete(a:listf)
		call delete(a:typef)
	endif

	" Drop removed buffers regardless of errors
	if get(g:, 'vifm_drop_gone_buffers', 0)
		call s:DropGoneBuffers(a:bufsnapshot, opened_bufnrs)
	endif
endfunction

function! s:PreparePath(path) abort
	let path = substitute(a:path, '\', '/', 'g')
	if !isdirectory(path)
		" For example, we were' in a terminal buffer whose name isn't a path
		let path = ''
	endif

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
function! s:DisplayVifmHelp() abort
	let runtimepath = &runtimepath
	let vimdoc = substitute(s:script_path, '[/\\]plugin[/\\].*', '', '')
	execute 'set runtimepath+='.vimdoc.'/../vim-doc'

	try
		execute 'help '.s:GetVifmHelpTopic()
	catch /E149:/
		let msg = substitute(v:exception, '[^:]\+:', '', '')
		return 'echoerr "'.escape(msg, '\"').'"'
	finally
		let &runtimepath = runtimepath
	endtry
	return ''
endfunction

function! s:GetVifmHelpTopic() abort
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

if get(g:, 'vifm_replace_netrw')
	function! s:HandleBufEnter(fname) abort
		if a:fname !=# '' && isdirectory(a:fname)
			if bufexists(0)
				buffer #
			else
				enew
			endif
			silent! bdelete! #

			let embed_split = get(g:, 'vifm_embed_split', 0)
			let g:vifm_embed_split = 0
			exec get(g:, 'vifm_replace_netrw_cmd', 'Vifm') . ' ' . a:fname
			let g:vifm_embed_split = embed_split
		endif
	endfunction

	augroup neovimvifm
		au BufEnter * silent call s:HandleBufEnter(expand('<amatch>'))
	augroup END
endif

" vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab :
