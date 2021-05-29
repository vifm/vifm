" vifm syntax file
" Maintainer:  xaizek <xaizek@posteo.net>
" Last Change: May 29, 2021

if exists('b:current_syntax')
	finish
endif

let b:current_syntax = 'vifm-rename'

let s:cpo_save = &cpo
set cpo-=C

" Whole line comment
syntax region vifmRenameComment contains=@Spell start='^#' end='$'

highlight link vifmRenameComment Comment

let &cpo = s:cpo_save
unlet s:cpo_save

" vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 :
