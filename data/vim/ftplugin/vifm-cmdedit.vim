" vifm command-line editing buffer filetype plugin
" Maintainer:  xaizek <xaizek@lavabit.com>
" Last Change: August 04, 2013

if exists("b:did_ftplugin")
	finish
endif

let b:did_ftplugin = 1

" Behave as vifm script file
runtime! ftplugin/vifm.vim

" Use vifm script highlighting
set syntax=vifm

" Mappings for quick leaving the buffer (behavior similar to Command line
" buffer in Vim)
nnoremap <buffer> <cr> :copy 0 \| wq<cr>
imap <buffer> <cr> <esc><cr>

" Start buffer editing in insert mode
startinsert

" vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 :
