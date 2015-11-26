" Mail file type extension to pick files for attachments via vifm
" Maintainer:  xaizek <xaizek@openmailbox.org>
" Last Change: March 30, 2015

" Insert attachment picked via vifm after 'Subject' header
function! s:AddMailAttacments()
	" TODO: reduce duplication between this file and plugins/vifm.vim
	let l:listf = tempname()
	if has('gui_running')
		execute 'silent !' g:vifm_term g:vifm_exec
		                 \ '--choose-files' shellescape(l:listf, 1) g:vifm_exec_args
	else
		execute 'silent !' g:vifm_exec
		                 \ '--choose-files' shellescape(l:listf, 1) g:vifm_exec_args
	endif

	redraw!

	if v:shell_error != 0
		echohl WarningMsg | echo 'Got non-zero code from vifm' | echohl None
		call delete(l:listf)
		return
	endif

	let l:insert_pos = search('^Subject:', 'nw')

	if filereadable(l:listf) && l:insert_pos != 0
		for line in readfile(l:listf)
			call append(l:insert_pos, 'Attach: '.line)
			let l:insert_pos += 1
		endfor
	endif
	call delete(l:listf)

	redraw!
endfunction

nnoremap <buffer> <silent> <localleader>a :call <sid>AddMailAttacments()<cr>

" vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 :
