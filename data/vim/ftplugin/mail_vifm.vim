" Mail file type extension to pick files for attachments via vifm
" Maintainer:  xaizek <xaizek@openmailbox.org>
" Last Change: October 05, 2014

" Insert attachment picked via vifm after 'Subject' header
function! s:AddMailAttacments()
	" TODO: reduce duplication between this file and plugins/vifm.vim
	if has('gui_running')
		execute 'silent !' g:vifm_term g:vifm_exec '-f' g:vifm_exec_args
	else
		execute 'silent !' g:vifm_exec '-f' g:vifm_exec_args
	endif

	let l:vimfiles = expand(g:vifm_home.'/vimfiles')
	let l:insert_pos = search('^Subject:', 'nw')

	if filereadable(l:vimfiles) && l:insert_pos != 0
		for line in readfile(l:vimfiles)
			call append(l:insert_pos, 'Attach: '.line)
			let l:insert_pos += 1
		endfor
	endif

	redraw!
endfunction

nnoremap <buffer> <silent> <localleader>a :call <sid>AddMailAttacments()<cr>

" vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 :
