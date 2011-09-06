" vifm syntax file
" Maintainer:  xaizek <xaizek@gmail.com>
" Last Change: September 3, 2011
" Based On:    Vim syntax file by Dr. Charles E. Campbell, Jr.

if exists('b:current_syntax')
	finish
endif

let b:current_syntax = 'vifm'

let s:cpo_save = &cpo
set cpo-=C

" General commands
syntax keyword vifmCommand contained apropos cd change colo[rscheme] d[elete]
		\ delm[arks] di[splay] dirs e[dit] empty exi[t] file filter fin[d] gr[ep]
		\ h[elp] hi[ghlight] his[tory] invert jobs locate ls marks mkdir
		\ noh[lsearch] on[ly] popd pushd pwd q[uit] reg[isters] rename restart
		\ restore screen se[t] sh[ell] sor[t] sp[lit] s[ubstitute] touch tr sync
		\ undol[ist] ve[rsion] vie[w] vifm w[rite] wq x[it] y[ank]

" Map commands
syntax keyword vifmMap contained cm[ap] cno[remap] cu[nmap] map nm[ap]
		\ nn[oremap] no[remap] nun[map] unm[ap] vm[ap] vn[oremap] vu[nmap]
		\ skipwhite nextgroup=vifmMapLhs

" Other commands
syntax keyword vifmCmdCommand contained com[mand]
syntax keyword vifmMarkCommand contained ma[rk]
syntax keyword vifmFtCommand contained filet[ype] filex[type] filev[iewer]

" Highlight groups
syntax keyword vifmHiArgs contained ctermfg ctermbg
syntax case ignore
syntax keyword vifmHiGroups contained Menu Border Win StatusBar CurrLine
		\ Directory Link Socket Device Executable Selected Current BrokenLink
		\ TopLine StatusLine Fifo ErrorMsg
syntax keyword vifmHiColors contained black red green yellow blue magenta cyan
		\ white default
syntax case match

" Options
syntax keyword vifmOption contained autochpos confirm cf fastrun followlinks
		\ fusehome history hi hlsearch hls iec ignorecase ic reversecol runexec
		\ scrolloff so shell sh smartcase scs sortnumbers timefmt timeoutlen trash
		\ undolevels ul vicmd vixcmd vifminfo vimhelp wildmenu wmnu wrap sort
		\ sortorder

" Disabled boolean options
syntax keyword vifmOption contained noautochpos noconfirm nocf nofastrun
		\ nofollowlinks nohlsearch nohls noiec noignorecase noic noreversecol
		\ norunexec nosmartcase noscs nosortnumbers notrash novimhelp nowildmenu
		\ nowmnu nowrap

" Inverted boolean options
syntax keyword vifmOption contained invautochpos invconfirm invcf invfastrun
		\ invfollowlinks invhlsearch invhls inviec invignorecase invic invreversecol
		\ invrunexec invsmartcase invscs invsortnumbers invtrash invvimhelp
		\ invwildmenu invwmnu invwrap

" Expressions
syntax region vifmStatement start='^\s*' skip='\(\n\s*\\\)\|\(\n\s*".*$\)'
		\ end='$' keepend
		\ contains=vifmCommand,vifmCmdCommandSt,vifmMarkCommandSt,vifmFtCommandSt
		\,vifmMap,vifmMapSt,vifmExecute,vifmCommands,vifmMapRhs,vifmComment
syntax region vifmCmdCommandSt start='^\s*com\%[mand]'
		\ skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$'
		\ keepend contains=vifmCmdCommand,vifmComment
syntax region vifmMarkCommandSt start='^\s*ma\%[rk]\>' end='$' keepend oneline
		\ contains=vifmMarkCommand
syntax region vifmFtCommandSt start='^\s*file[tvx]'
		\ skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$' keepend
		\ contains=vifmFtCommand,vifmComment
syntax region vifmExecute start='!' end='$' keepend oneline
		\ contains=vifmNotation
syntax region vifmCommands start=':' end='$' keepend oneline
		\ contains=vifmCommand,vifmNotation,vifmExecute
syntax match vifmMapLhs /\S\+/ contained contains=vifmNotation
		\ nextgroup=vifmMapRhs
syntax match vifmMapRhs /\s\+\S\+/ contained
		\ contains=vifmNotation,vifmCommand,vifmExecute
syntax region vifmHi matchgroup=vifmCommand
		\ start='\<hi\%[ghlight]\>' skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$'
		\ keepend contains=vifmHiArgs,vifmHiGroups,vifmHiColors,vifmNumber
syntax region vifmSet matchgroup=vifmCommand
		\ start='\<se\%[t]\>' skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$' keepend
		\ contains=vifmOption,vifmSetString,vifmNumber,vifmComment
syntax region vifmSetString contained start=+="+hs=s+1 skip=+\\\\\|\\"+  end=+"+
syntax region vifmSetString contained start=+='+hs=s+1 skip=+\\\\\|\\'+  end=+'+
syntax match vifmNumber contained /\d\+/

" Ange-bracket notation
syntax case ignore
syntax match vifmNotation '<\(cr\|space\|f\d\{1,2\}\|c-[a-z[\]^_]\)>'
syntax case match

" Whole line comments
syntax region vifmComment contained start="^\s*\"" end="$"

" Empty line
syntax match vifmEmpty /^\s*$/

" Highlight
highlight link vifmComment Comment
highlight link vifmCommand Statement
highlight link vifmCmdCommand Statement
highlight link vifmMarkCommand Statement
highlight link vifmFtCommand Statement
highlight link vifmMap Statement
highlight link vifmHiArgs Type
highlight link vifmHiGroups PreProc
highlight link vifmHiColors Special
highlight link vifmOption PreProc
highlight link vifmNotation Special
highlight link vifmSetString String
highlight link vifmNumber Number

let &cpo = s:cpo_save
unlet s:cpo_save

" vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 :
