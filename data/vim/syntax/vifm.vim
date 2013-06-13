" vifm syntax file
" Maintainer:  xaizek <xaizek@lavabit.com>
" Last Change: June 13, 2013
" Based On:    Vim syntax file by Dr. Charles E. Campbell, Jr.

if exists('b:current_syntax')
	finish
endif

let b:current_syntax = 'vifm'

let s:cpo_save = &cpo
set cpo-=C

" General commands
syntax keyword vifmCommand contained alink apropos cd change chmod chown clone
		\ co[py] d[elete] delm[arks] di[splay] dirs e[dit] el[se] empty en[dif]
		\ exi[t] file filter fin[d] fini[sh] gr[ep] h[elp] hi[ghlight] his[tory]
		\ invert jobs let locate ls marks mes[sages] mkdir m[ove] noh[lsearch]
		\ on[ly] popd pushd pwd q[uit] reg[isters] rename restart restore rlink
		\ screen se[t] sh[ell] sor[t] so[urce] sp[lit] s[ubstitute] touch tr sync
		\ undol[ist] unl[et] ve[rsion] vie[w] vifm windo winrun w[rite] wq x[it]
		\ y[ank]

" Map commands
syntax keyword vifmMap contained cm[ap] cno[remap] cu[nmap] map mm[ap]
		\ mn[oremap] mu[nmap] nm[ap] nn[oremap] no[remap] nun[map] qm[ap] qn[oremap]
		\ qun[map] unm[ap] vm[ap] vn[oremap] vu[nmap]
		\ skipwhite nextgroup=vifmMapArgs

" Other commands
syntax keyword vifmCmdCommand contained com[mand]
syntax keyword vifmColoCommand contained colo[rscheme]
syntax keyword vifmMarkCommand contained ma[rk]
syntax keyword vifmFtCommand contained filet[ype] filex[type] filev[iewer]
syntax keyword vifmExprCommand contained if ec[ho] exe[cute]
syntax keyword vifmNormalCommand contained norm[al]

" Builtin functions
syntax match vifmBuiltinFunction '\(filetype\|expand\)\ze('

" Operators
syntax match vifmOperator "\(==\|!=\)" skipwhite

" Highlight groups
syntax keyword vifmHiArgs contained cterm ctermfg ctermbg
syntax case ignore
syntax keyword vifmHiGroups contained WildMenu Border Win CmdLine CurrLine
		\ Directory Link Socket Device Executable Selected Current BrokenLink
		\ TopLine TopLineSel StatusLine Fifo ErrorMsg
syntax keyword vifmHiStyles contained bold underline reverse inverse standout
		\ none
syntax keyword vifmHiColors contained black red green yellow blue magenta cyan
		\ white default lightblack lightred lightgreen lightyellow lightblue
		\ lightmagenta lightcyan lightwhite
syntax case match

" Options
syntax keyword vifmOption contained autochpos classify columns co confirm cf
		\ cpoptions cpo dotdirs fastrun followlinks fusehome gdefault history hi
		\ hlsearch hls iec ignorecase ic incsearch is laststatus lines ls lsview
		\ rulerformat ruf runexec scrollbind scb scrolloff so sort sortorder shell
		\ sh shortmess shm slowfs smartcase scs sortnumbers statusline stl tabstop
		\ timefmt timeoutlen trash trashdir ts undolevels ul vicmd viewcolumns
		\ vifminfo vimhelp vixcmd wildmenu wmnu wrap wrapscan ws

" Disabled boolean options
syntax keyword vifmOption contained noautochpos noconfirm nocf nofastrun
		\ nofollowlinks nohlsearch nohls noiec noignorecase noic noincsearch nois
		\ nolaststatus nols nolsview noscrollbind noscb norunexec nosmartcase noscs
		\ nosortnumbers notrash novimhelp nowildmenu nowmnu nowrap nowrapscan nows

" Inverted boolean options
syntax keyword vifmOption contained invautochpos invconfirm invcf invfastrun
		\ invfollowlinks invhlsearch invhls inviec invignorecase invic invincsearch
		\ invis invlaststatus invls invlsview invscrollbind invscb invrunexec
		\ invsmartcase invscs invsortnumbers invtrash invvimhelp invwildmenu invwmnu
		\ invwrap invwrapscan invws

" Expressions
syntax region vifmStatement start='^\(\s\|:\)*' skip='\(\n\s*\\\)\|\(\n\s*".*$\)'
		\ end='$' keepend
		\ contains=vifmCommand,vifmCmdCommandSt,vifmMarkCommandSt,vifmFtCommandSt
		\,vifmMap,vifmMapSt,vifmExecute,vifmComment,vifmExprCommandSt
		\,vifmNormalCommandSt
syntax region vifmStatementC start='\(\s\|:\)*' skip='\(\n\s*\\\)\|\(\n\s*".*$\)'
		\ end='$' keepend
		\ contained
		\ contains=vifmCommand,vifmCmdCommandSt,vifmMarkCommandSt,vifmFtCommandSt
		\,vifmMap,vifmMapSt,vifmExecute,vifmComment,vifmExprCommandSt
		\,vifmNormalCommandSt,vifmNotation
syntax region vifmCmdCommandSt start='^\(\s\|:\)*com\%[mand]'
		\ skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$'
		\ keepend contains=vifmCmdCommand,vifmComment
syntax region vifmColoCommandSt start='^\(\s\|:\)*colo\%[rscheme]\>' end='$' keepend
		\ oneline contains=vifmColoCommand
syntax region vifmMarkCommandSt start='^\(\s\|:\)*ma\%[rk]\>' end='$' keepend oneline
		\ contains=vifmMarkCommand
syntax region vifmFtCommandSt start='^\(\s\|:\)*file[tvx]'
		\ skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$' keepend
		\ contains=vifmFtCommand,vifmComment
syntax region vifmMapSt start='^\(\s\|:\)*\(cm\%[ap]\|cno\%[remap]\|cu\%[nmap]\|map\|mm\%[ap]\|mn\%[oremap]\|mu\%[nmap]\|nm\%[ap]\|nn\%[oremap]\|no\%[remap]\|nun\%[map]\|qm\%[ap]\|qn\%[oremap]\|qun\%[map]\|unm\%[ap]\|vm\%[ap]\|vn\%[oremap]\|vu\%[nmap]\)'
		\ skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$' keepend
		\ contains=vifmMap
syntax region vifmExprCommandSt start='\<\(if\|ec\%[ho]\|exe\%[cute]\)\>'
		\ end='$' keepend
		\ contains=vifmExprCommand,vifmString,vifmStringInExpr,vifmBuiltinFunction
		\,vifmOperator,vifmEnvVar
syntax region vifmNormalCommandSt start='^\(\s\|:\)*norm\%[al]\>' end='$' keepend
		\ oneline
		\ contains=vifmNormalCommand
syntax region vifmExecute start='!' end='$' keepend oneline
		\ contains=vifmNotation
syntax region vifmMapArgs start='\S\+'
		\ end='\n\s*\\' skip='\(\n\s*\\\)\|\(\n\s*".*$\)'
		\ contained
		\ contains=vifmNotation,vifmMapRhs
syntax region vifmMapRhs start='\s*:\s*\S\+'
		\ end='$'
		\ contained
		\ contains=vifmStatementC
syntax region vifmHi matchgroup=vifmCommand
		\ start='^\(\s\|:\)*\<hi\%[ghlight]\>' skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$'
		\ keepend
		\ contains=vifmHiArgs,vifmHiGroups,vifmHiStyles,vifmHiColors,vifmNumber
		\,vifmComment
syntax region vifmSet
		\ matchgroup=vifmCommand
		\ start='^\(\s\|:\)*\<se\%[t]\>' skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$'
		\ keepend contains=vifmOption,vifmString,vifmNumber,vifmComment
syntax region vifmSet2 contained
		\ matchgroup=vifmCommand
		\ start='^\(\s\|:\)*\<se\%[t]\>' skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$' keepend
		\ contains=vifmOption,vifmString,vifmNumber,vifmComment,vifmNotation
syntax region vifmLet
		\ matchgroup=vifmCommand
		\ start='^\(\s\|:\)*\<let\>' skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$'
		\ keepend contains=vifmEnvVar,vifmString,vifmStringInExpr
syntax region vifmString contained start=+="+hs=s+1 skip=+\\\\\|\\"+  end=+"+
syntax region vifmString contained start=+='+hs=s+1 skip=+\\\\\|\\'+  end=+'+
syntax region vifmStringInExpr contained start=+=\@<="+hs=s+1 skip=+\\\\\|\\"+  end=+"+
syntax region vifmStringInExpr contained start=+=\@<='+hs=s+1 skip=+\\\\\|\\'+  end=+'+
syntax region vifmStringInExpr contained start=+[.( ]"+hs=s+1 skip=+\\\\\|\\"+  end=+"+
syntax region vifmStringInExpr contained start=+[.( ]'+hs=s+1 skip=+\\\\\|\\'+  end=+'+
syntax match vifmEnvVar contained /\$[0-9a-zA-Z_]\+/
syntax match vifmNumber contained /\d\+/

" Ange-bracket notation
syntax case ignore
syntax match vifmNotation '<\(esc\|cr\|space\|del\|\(s-\)\?tabhome\|end\|left\|right\|up\|down\|bs\|delete\|pageup\|pagedown\|tab\|s-tab\|\([acms]-\)\?f\d\{1,2\}\|c-s-[a-z[\]^_]\|s-c-[a-z[\]^_]\|c-[a-z[\]^_]\|[am]-c-[a-z]\|c-[am]-[a-z]\|[am]-[a-z]\)>'
syntax case match

" Whole line comments
syntax region vifmComment contained start='^\(\s\|:\)*"' end='$'

" Empty line
syntax match vifmEmpty /^\s*$/

" Highlight
highlight link vifmComment Comment
highlight link vifmCommand Statement
highlight link vifmCmdCommand Statement
highlight link vifmColoCommand Statement
highlight link vifmMarkCommand Statement
highlight link vifmFtCommand Statement
highlight link vifmExprCommand Statement
highlight link vifmNormalCommand Statement
highlight link vifmBuiltinFunction Function
highlight link vifmOperator Operator
highlight link vifmMap Statement
highlight link vifmHiArgs Type
highlight link vifmHiGroups Identifier
highlight link vifmHiStyles PreProc
highlight link vifmHiColors Special
highlight link vifmOption PreProc
highlight link vifmNotation Special
highlight link vifmString String
highlight link vifmStringInExpr String
highlight link vifmEnvVar PreProc
highlight link vifmNumber Number

let &cpo = s:cpo_save
unlet s:cpo_save

" vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 :
