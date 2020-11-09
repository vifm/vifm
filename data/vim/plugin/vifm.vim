" Maintainer: Ken Steen <ksteen@users.sourceforge.net>
" Last Change: 2001 November 29

" Maintainer: xaizek <xaizek@posteo.net>
" Last Change: 2020 July 1

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
			buffer #
		endif
		silent! bdelete! #
		if data.split
			silent! close
		endif
		if has('job') && type(data.cwdjob) == v:t_job
			call job_stop(data.cwdjob)
		endif
		call s:HandleRunResults(a:code, data.listf, data.typef, data.editcmd)
	endfunction
endif

function! s:StartVifm(mods, count, editcmd, ...) abort
	if a:0 > 2
		echoerr 'Too many arguments'
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

	" Use embedded terminal if available.
	if has('nvim') || exists('*term_start') && g:vifm_embed_term
		let [cwdf, cwdjob] = s:StartCwdJob()

		if cwdf != ''
			let cwdargs = '-c "autocmd DirEnter * !pwd >> ' . shellescape(cwdf) . '"'
		else
			let cwdargs = ''
		endif

		let data = { 'listf' : listf, 'typef' : typef, 'editcmd' : a:editcmd,
					\ 'cwdjob' : cwdjob, 'split': get(g:, 'vifm_embed_split', 0) }

		if !has('nvim')
			let env = { 'TERM' : has('gui_running') ? $TERM :
			          \          &term =~ 256 ? 'xterm-256color' : &term }
			let options = { 'term_name' : 'vifm: '.a:editcmd, 'curwin' : 1,
			              \ 'exit_cb': funcref('VifmExitCb', [data]), 'env' : env }
		else
			function! data.on_exit(id, code, event) abort
				if (bufnr('%') == bufnr('#') || !bufexists(0)) && !self.split
					enew
				else
					buffer #
				endif
				silent! bdelete! #
				if self.split
					silent! close
				endif
				if self.cwdjob != 0
					call jobstop(self.cwdjob)
				endif
				call s:HandleRunResults(a:code, self.listf, self.typef, self.editcmd)
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
			execute 'keepalt file' escape('vifm: '.a:editcmd, ' |')
			setlocal nonumber norelativenumber
			execute bufnr(oldbuf).'bwipeout'
			" Use execute to not break highlighting.
			execute 'startinsert'
		endif

		return
	else
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
	endif
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

function! s:HandleRunResults(exitcode, listf, typef, editcmd) abort
	if a:exitcode != 0
		echoerr 'Got non-zero code from vifm: ' . a:exitcode
		call delete(a:listf)
		call delete(a:typef)
		return
	endif

	" The selected files are written and read from a file instead of using
	" vim's clientserver so that it will work in the console without a X server
	" running.

	if !file_readable(a:listf)
		echoerr 'Failed to read list of files'
		call delete(a:listf)
		call delete(a:typef)
		return
	endif

	let flist = readfile(a:listf)
	call delete(a:listf)

	let opentype = file_readable(a:typef) ? readfile(a:typef) : []
	call delete(a:typef)

	" User exits vifm without selecting a file.
	if empty(flist)
		echohl WarningMsg | echo 'No file selected' | echohl None
		return
	endif

	let unescaped_firstfile = flist[0]
	call map(flist, 'fnameescape(v:val)')
	let firstfile = flist[0]

	if !empty(opentype) && !empty(opentype[0]) &&
		\ opentype[0] != '"%VIFM_OPEN_TYPE%"'
		let editcmd = has('win32') ? opentype[0][1:-2] : opentype[0]
	else
		let editcmd = a:editcmd
	endif

	" Don't split if current window is empty
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
	if editcmd == 'edit' || !s:has_drop
		" Linked folders must be resolved to successfully call 'buffer'
		let firstfile = unescaped_firstfile
		let firstfile = resolve(fnamemodify(firstfile, ':h'))
					\ .'/'.fnamemodify(firstfile, ':t')
		let firstfile = fnameescape(firstfile)
		execute 'buffer' fnamemodify(firstfile, ':.')
	elseif s:has_drop
		" Mind that drop replaces arglist, so don't use it with :edit.
		execute 'drop' firstfile
	endif
endfunction

function! s:PreparePath(path) abort
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
			buffer #
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
