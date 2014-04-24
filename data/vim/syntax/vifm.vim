" vifm syntax file
" Maintainer:  xaizek <xaizek@openmailbox.org>
" Last Change: April 24, 2014
" Based On:    Vim syntax file by Dr. Charles E. Campbell, Jr.

if exists('b:current_syntax')
	finish
endif

let b:current_syntax = 'vifm'

let s:cpo_save = &cpo
set cpo-=C

" General commands
syntax keyword vifmCommand contained alink apropos change chmod chown clone
		\ co[py] d[elete] delm[arks] di[splay] dirs e[dit] el[se] empty en[dif]
		\ exi[t] file filter fin[d] fini[sh] gr[ep] h[elp] his[tory] jobs locate ls
		\ lstrash marks mes[sages] mkdir m[ove] noh[lsearch] on[ly] popd pushd pwd
		\ q[uit] reg[isters] rename restart restore rlink screen sh[ell] sor[t]
		\ sp[lit] s[ubstitute] touch tr trashes sync undol[ist] ve[rsion] vie[w]
		\ vifm vs[plit] windo winrun w[rite] wq x[it] y[ank]

" Map commands
syntax keyword vifmMap contained map mm[ap] mn[oremap] mu[nmap] nm[ap]
		\ nn[oremap] no[remap] nun[map] qm[ap] qn[oremap] qun[map] unm[ap] vm[ap]
		\ vn[oremap] vu[nmap] skipwhite nextgroup=vifmMapArgs
syntax keyword vifmCMap contained cm[ap] cno[remap] cu[nmap]
		\ skipwhite nextgroup=vifmCMapArgs

" Other commands
syntax keyword vifmCdCommand contained cd
syntax keyword vifmCmdCommand contained com[mand] nextgroup=vifmCmdCommandName
syntax keyword vifmColoCommand contained colo[rscheme]
syntax keyword vifmHiCommand contained hi[ghlight]
syntax keyword vifmInvertCommand contained invert
syntax keyword vifmLetCommand contained let
syntax keyword vifmUnletCommand contained unl[et]
syntax keyword vifmSetCommand contained se[t]
syntax keyword vifmSoCommand contained so[urce]
syntax keyword vifmMarkCommand contained ma[rk]
syntax keyword vifmFtCommand contained filet[ype] filex[type] filev[iewer]
syntax keyword vifmExprCommand contained if ec[ho] exe[cute]
syntax keyword vifmNormalCommand contained norm[al]
		\ nextgroup=vifmColonSubcommand

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
syntax keyword vifmOption contained aproposprg autochpos classify columns co
		\ confirm cf cpoptions cpo dotdirs fastrun findprg followlinks fusehome
		\ gdefault grepprg history hi hlsearch hls iec ignorecase ic incsearch is
		\ laststatus lines locateprg ls lsview number nu numberwidth nuw
		\ relativenumber rnu rulerformat ruf runexec scrollbind scb scrolloff so
		\ sort sortorder shell sh shortmess shm slowfs smartcase scs sortnumbers
		\ statusline stl tabstop timefmt timeoutlen trash trashdir ts undolevels ul
		\ vicmd viewcolumns vifminfo vimhelp vixcmd wildmenu wmnu wrap wrapscan ws

" Disabled boolean options
syntax keyword vifmOption contained noautochpos noconfirm nocf nofastrun
		\ nofollowlinks nohlsearch nohls noiec noignorecase noic noincsearch nois
		\ nolaststatus nols nolsview nonumber nonu norelativenumber nornu
		\ noscrollbind noscb norunexec nosmartcase noscs nosortnumbers notrash
		\ novimhelp nowildmenu nowmnu nowrap nowrapscan nows

" Inverted boolean options
syntax keyword vifmOption contained invautochpos invconfirm invcf invfastrun
		\ invfollowlinks invhlsearch invhls inviec invignorecase invic invincsearch
		\ invis invlaststatus invls invlsview invnumber invnu invrelativenumber
		\ invrnu invscrollbind invscb invrunexec invsmartcase invscs invsortnumbers
		\ invtrash invvimhelp invwildmenu invwmnu invwrap invwrapscan invws

" Expressions
syntax region vifmStatement start='^\(\s\|:\)*'
		\ skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$' keepend
		\ contains=vifmCommand,vifmCmdCommand,vifmCmdCommandSt,vifmMarkCommandSt
		\,vifmFtCommandSt,vifmCMap,vifmMap,vifmMapSt,vifmCMapSt,vifmExecute
		\,vifmComment,vifmExprCommandSt,vifmNormalCommandSt,vifmCdCommandSt,vifmSet
		\,vifmArgument,vifmSoCommandSt
" Contained statement without highlighting of angle-brace notation.
syntax region vifmStatementCN start='\(\s\|:\)*'
		\ skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$' keepend contained
		\ contains=vifmCommand,vifmCmdCommand,vifmCmdCommandSt,vifmMarkCommandSt
		\,vifmFtCommandStN,vifmCMap,vifmMap,vifmMapSt,vifmCMapSt,vifmExecute
		\,vifmComment,vifmExprCommandSt,vifmNormalCommandSt,vifmNotation
		\,vifmCdCommandStN,vifmSetN,vifmArgument,vifmSoCommand,vifmSoCommandStN
		\,vifmInvertCommand,vifmInvertCommandStN
" Contained statement with highlighting of angle-brace notation.
syntax region vifmStatementC start='\(\s\|:\)*'
		\ skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$' keepend contained
		\ contains=vifmCommand,vifmCmdCommand,vifmCmdCommandSt,vifmMarkCommandSt
		\,vifmFtCommandSt,vifmCMap,vifmMap,vifmMapSt,vifmCMapSt,vifmExecute
		\,vifmComment,vifmExprCommandSt,vifmNormalCommandSt,vifmCdCommandSt,vifmSet
		\,vifmArgument,vifmSoCommand,vifmSoCommandSt,vifmInvertCommand
		\,vifmInvertCommandSt
syntax region vifmCmdCommandSt start='^\(\s\|:\)*com\%[mand]'
		\ skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$' keepend
		\ contains=vifmCmdCommand,vifmComment
syntax region vifmCmdCommandName contained start='!\?\s\+[a-zA-Z]\+' end='\ze\s'
		\ skip='\(\s*\\\)\|\(\s*".*$\)'
		\ nextgroup=vifmCmdArgs
syntax region vifmCmdArgs start='\(\s*\n\s*\\\)\?\s*\S\+'
		\ end='\s' skip='\(\n\s*\\\)\|\(\n\s*".*$\)'
		\ contained
		\ contains=vifmColonSubcommand,vifmComment
syntax region vifmColoCommandSt start='^\(\s\|:\)*colo\%[rscheme]\>' end='$'
		\ keepend oneline contains=vifmColoCommand
syntax region vifmInvertCommandSt start='\(\s\|:\)*invert\>' end='$\||'
		\ keepend oneline contains=vifmInvertCommand
syntax region vifmInvertCommandStN start='\(\s\|:\)*invert\>' end='$\||'
		\ contained keepend oneline contains=vifmInvertCommand,vifmNotation
syntax region vifmSoCommandSt start='\(\s\|:\)*so\%[urce]\>' end='$\||'
		\ keepend oneline contains=vifmSoCommand,vifmStringInExpr
syntax region vifmSoCommandStN start='\(\s\|:\)*so\%[urce]\>' end='$\||'
		\ contained keepend oneline
		\ contains=vifmSoCommand,vifmNotation,vifmStringInExpr
syntax region vifmMarkCommandSt start='^\(\s\|:\)*ma\%[rk]\>' end='$' keepend
		\ oneline contains=vifmMarkCommand
syntax region vifmCdCommandSt start='\(\s\|:\)*cd\>' end='$\||' keepend oneline
		\ contains=vifmCdCommand,vifmEnvVar,vifmStringInExpr
" Highlight for :cd command with highlighting of angle-brace notation.
syntax region vifmCdCommandStN start='\(\s\|:\)*cd\>' end='$\||' keepend oneline
		\ contained
		\ contains=vifmCdCommand,vifmEnvVar,vifmNotation,vifmStringInExpr
syntax region vifmFtCommandSt start='\(\s\|:\)*file[tvx]'
		\ skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$' keepend
		\ contains=vifmFtCommand,vifmComment
syntax region vifmFtCommandStN start='\(\s\|:\)*file[tvx]'
		\ skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$\|\(<[cC][rR]>\)' keepend
		\ contains=vifmFtCommand,vifmComment,vifmNotation
syntax region vifmMapSt start='^\(\s\|:\)*\(map\|mm\%[ap]\|mn\%[oremap]\|mu\%[nmap]\|nm\%[ap]\|nn\%[oremap]\|no\%[remap]\|nun\%[map]\|qm\%[ap]\|qn\%[oremap]\|qun\%[map]\|unm\%[ap]\|vm\%[ap]\|vn\%[oremap]\|vu\%[nmap]\)'
		\ skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$' keepend
		\ contains=vifmMap
syntax region vifmCMapSt
		\ start='^\(\s\|:\)*\(cm\%[ap]\|cno\%[remap]\|cu\%[nmap]\)'
		\ skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$' keepend
		\ contains=vifmCMap
syntax region vifmExprCommandSt start='\<\(if\|ec\%[ho]\|exe\%[cute]\)\>'
		\ end='$\||'
		\ contains=vifmExprCommand,vifmString,vifmStringInExpr,vifmBuiltinFunction
		\,vifmOperator,vifmEnvVar
syntax region vifmNormalCommandSt start='\(\s\|:\)*norm\%[al]\>' end='$' keepend
		\ oneline
		\ contains=vifmNormalCommand
syntax region vifmExecute start='!' end='$' keepend oneline
		\ contains=vifmNotation
syntax region vifmMapArgs start='\S\+'
		\ end='\n\s*\\' skip='\(\n\s*\\\)\|\(\n\s*".*$\)'
		\ contained
		\ contains=vifmMapLhs,vifmMapRhs
syntax region vifmCMapArgs start='\S\+'
		\ end='\n\s*\\' skip='\(\n\s*\\\)\|\(\n\s*".*$\)'
		\ contained
		\ contains=vifmMapLhs,vifmMapCRhs
syntax region vifmMapLhs start='\S\+'
		\ end='\ze\s' skip='\(\s*\\\)\|\(\s*".*$\)'
		\ contained
		\ contains=vifmNotation,vifmComment
		\ nextgroup=vifmColonSubcommandN
syntax region vifmMapRhs start='\s'
		\ end='<cr>' skip='\(\s*\\\)\|\(\s*".*$\)'
		\ contained keepend
		\ contains=vifmNotation,vifmComment,vifmColonSubcommandN
syntax region vifmMapCRhs start='\s'
		\ end='<cr>' skip='\(\s*\\\)\|\(\s*".*$\)'
		\ contained keepend
		\ contains=vifmNotation,vifmComment,vifmSubcommandN
syntax region vifmColonSubcommand start='\s*\(\s*\n\s*\\\)\?:\s*\S\+'
		\ end='$' skip='\s*\n\(\s*\\\)\|\(\s*".*$\)'
		\ contained
		\ contains=vifmStatementC
" Contained sub command with highlighting of angle-brace notation.
syntax region vifmColonSubcommandN start='\s*\(\s*\n\s*\\\)\?:\s*\S\+'
		\ end='$' skip='\s*\n\(\s*\\\)\|\(\s*".*$\)'
		\ contained
		\ contains=vifmStatementCN
syntax region vifmSubcommandN start='\s*\(\s*\n\s*\\\)\?:\?\s*\S\+'
		\ end='$' skip='\s*\n\(\s*\\\)\|\(\s*".*$\)'
		\ contained
		\ contains=vifmStatementCN
syntax region vifmHi
		\ start='^\(\s\|:\)*\<hi\%[ghlight]\>' skip='\(\n\s*\\\)\|\(\n\s*".*$\)'
		\ end='$' keepend
		\ contains=vifmHiCommand,vifmHiArgs,vifmHiGroups,vifmHiStyles,vifmHiColors
		\,vifmNumber,vifmComment
syntax region vifmSet
		\ start='\(\s\|:\)*\<se\%[t]\>' skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$'
		\ keepend
		\ contains=vifmSetCommand,vifmOption,vifmString,vifmNumber,vifmComment
syntax region vifmSetN
		\ start='\(\s\|:\)*\<se\%[t]\>' skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$'
		\ keepend
		\ contains=vifmSetCommand,vifmOption,vifmString,vifmNumber,vifmComment
		\,vifmNotation
syntax region vifmSet2 contained
		\ start='^\(\s\|:\)*\<se\%[t]\>' skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$'
		\ keepend
		\ contains=vifmSetCommand,vifmOption,vifmString,vifmNumber,vifmComment
		\,vifmNotation
syntax region vifmLet
		\ start='^\(\s\|:\)*\<let\>' skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$'
		\ keepend
		\ contains=vifmLetCommand,vifmEnvVar,vifmString,vifmStringInExpr,vifmComment
syntax region vifmUnlet
		\ start='^\(\s\|:\)*\<unl\%[et]\>' skip='\(\n\s*\\\)\|\(\n\s*".*$\)' end='$'
		\ keepend
		\ contains=vifmUnletCommand,vifmEnvVar,vifmComment
syntax region vifmString contained start=+="+hs=s+1 skip=+\\\\\|\\"+  end=+"+
syntax region vifmString contained start=+='+hs=s+1 skip=+\\\\\|\\'+  end=+'+
syntax region vifmStringInExpr contained start=+=\@<="+hs=s+1 skip=+\\\\\|\\"+
		\ end=+"+
syntax region vifmStringInExpr contained start=+=\@<='+hs=s+1
		\ skip=+\\\\\|\\'\|''+  end=+'+
syntax region vifmStringInExpr contained start=+[.( ]"+hs=s+1 skip=+\\\\\|\\"+
		\ end=+"+
syntax region vifmStringInExpr contained start=+[.( ]'+hs=s+1
		\ skip=+\\\\\|\\'\|''+  end=+'+
syntax region vifmArgument contained start=+"+ skip=+\\\\\|\\"+  end=+"+
syntax region vifmArgument contained start=+'+ skip=+\\\\\|\\'\|''+  end=+'+
syntax match vifmEnvVar contained /\$[0-9a-zA-Z_]\+/
syntax match vifmNumber contained /\d\+/

" Ange-bracket notation
syntax case ignore
syntax match vifmNotation '<\(esc\|cr\|space\|del\|nop\|\(s-\)\?tab\|home\|end\|left\|right\|up\|down\|bs\|delete\|pageup\|pagedown\|\([acms]-\)\?f\d\{1,2\}\|c-s-[a-z[\]^_]\|s-c-[a-z[\]^_]\|c-[a-z[\]^_]\|[am]-c-[a-z]\|c-[am]-[a-z]\|[am]-[a-z]\)>'
syntax case match

" Whole line comments
syntax region vifmComment contained start='^\(\s\|:\)*"' end='$'

" Empty line
syntax match vifmEmpty /^\s*$/

" Highlight
highlight link vifmComment Comment
highlight link vifmCommand Statement
highlight link vifmCdCommand Statement
highlight link vifmCmdCommand Statement
highlight link vifmColoCommand Statement
highlight link vifmHiCommand Statement
highlight link vifmInvertCommand Statement
highlight link vifmMarkCommand Statement
highlight link vifmFtCommand Statement
highlight link vifmExprCommand Statement
highlight link vifmNormalCommand Statement
highlight link vifmLetCommand Statement
highlight link vifmUnletCommand Statement
highlight link vifmSetCommand Statement
highlight link vifmSoCommand Statement
highlight link vifmBuiltinFunction Function
highlight link vifmOperator Operator
highlight link vifmMap Statement
highlight link vifmCMap Statement
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
