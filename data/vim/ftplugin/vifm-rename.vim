" Filetype plugin for vifm rename buffer
" Maintainer:  xaizek <xaizek@posteo.net>
" Last Change: May 29, 2021

if exists("b:did_ftplugin")
	finish
endif

let b:did_ftplugin = 1

" Finds zero-based range of lines that contain "# Original names:" through
" "# Edited names:" (included)
function s:FindSplitPos()
	let l:alllines = getline(1, '$')
	let l:start = -1
	let l:end = -1

	for i in range(0, len(l:alllines) - 1)
		if l:alllines[i][0:1] != '# '
			break
		endif

		if l:alllines[i] == '# Original names:'
			if l:start != -1
				return [ -1, -1 ]
			endif
			let l:start = i
		endif

		if l:alllines[i] == '# Edited names:'
			if l:end != -1
				return [ -1, -1 ]
			endif
			let l:end = i
		endif
	endfor

	if l:start == -1 || l:end == -1 || l:start > l:end
		return [ -1, -1 ]
	endif

	for j in range(i, len(l:alllines) - 1)
		if l:alllines[j][0] == '#'
			return [ -1, -1 ]
		endif
	endfor

	let l:names = len(l:alllines) - i
	if len(l:alllines) < l:names*2 + 2
		return [ -1, -1 ]
	endif

	return [ l:start, l:end ]
endfunction

" Extract list of original file names
let [ s:from, s:to ] = s:FindSplitPos()
if s:from >= 0
	let s:files = map(getline(0, 1 + s:from - 1), '"#"')
	          \ + map(getline(1 + s:from + 1, s:to), 'v:val[3:]')
	" Remove comments
	silent! execute (s:from + 1).'delete' (s:to - s:from + 1)
else
	let s:files = getline(1, '$')
endif

" Closes window/tab/Vim when buffer is left alone in there
function! s:QuitIfOnlyWindow()
	" Boil out if there is more than one window
	if winbufnr(2) != -1
		return
	endif

	" Just close tab with this single window or quit Vim with last tab
	if tabpagenr('$') == 1
		bdelete
		quit
	else
		close
	endif
endfunction

" Create a vertical split window for original file names and configure it
belowright vsplit __VifmRenameOrig__
enew
call setline(1, s:files)
setlocal buftype=nofile
setlocal bufhidden=hide
setlocal noswapfile
setlocal nobuflisted
setlocal cursorbind
setlocal scrollbind
setlocal nocursorline
setlocal syntax=vifm-rename

" Free now useless list of file names
unlet s:files

" Setup a hook in auxiliary local window to do not leave it alone, when it's
" useless
augroup VifmRenameAutoCmds
	autocmd! * <buffer>
	autocmd BufEnter <buffer> call s:QuitIfOnlyWindow()
augroup END

" Go back to the original window and ensure it will remain synchronized with
" the auxiliary one
wincmd w
setlocal cursorbind
setlocal scrollbind

" vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 :
