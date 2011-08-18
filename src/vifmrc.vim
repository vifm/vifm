" vifmrc syntax file
" Maintainer:  xaizek <xaizek@gmail.com>
" Last Change: August 18, 2011

if exists('b:current_syntax')
    finish
endif

let b:current_syntax = 'vifmrc'

" Whole line comments
syntax region vifmComment start="^\s*\"" end="$"

" Commands
syntax keyword vifmCommand contained apropos cd change colo[rscheme] d[elete]
      \ delm[arks] di[splay] dirs e[dit] empty exi[t] file filter fin[d] gr[ep]
      \ h[elp] his[tory] invert jobs locate ls marks noh[lsearch] on[ly] popd
      \ pushd pwd q[uit] reg[isters] rename restart screen se[t] sh[ell] sor[t]
      \ sp[lit] s[ubstitute] sync undol[ist] vie[w] vifm w[rite] wq x[it] y[ank]

syntax keyword vifmCmdCommand contained com[mand]

syntax keyword vifmMarkCommand contained ma[rk]

syntax keyword vifmFtCommand contained filet[ype] filex[type] filev[iewer]

" Options
syntax keyword vifmOption contained confirm cf fastrun followlinks fusehome
      \ history hi hlsearch hls iec ignorecase ic reversecol runexec shell sh
      \ smartcase scs sortnumbers timefmt trash undolevels ul vicmd vixcmd
      \ vifminfo vimhelp wildmenu wmnu wrap sort sortorder

" Disabled boolean options
syntax keyword vifmOption contained noconfirm nocf nofastrun nofollowlinks
      \ nohlsearch nohls noiec noignorecase noic noreversecol norunexec
      \ nosmartcase noscs nosortnumbers notrash novimhelp nowildmenu nowmnu
      \ nowrap

" Inverted boolean options
syntax keyword vifmOption contained invconfirm invcf invfastrun invfollowlinks
      \ invhlsearch invhls inviec invignorecase invic invreversecol invrunexec
      \ invsmartcase invscs invsortnumbers invtrash invvimhelp invwildmenu
      \ invwmnu invwrap

syntax keyword vifmMap contained cm[ap] cno[remap] cu[nmap] map nm[ap]
      \ nn[oremap] no[remap] nun[map] unm[ap] vm[ap] vn[oremap] vu[nmap]
      \ skipwhite nextgroup=vifmMapLhs

" Expressions
syntax region vifmStatement start='^\s*[^"]' skip='\n\s*\\' end='$' keepend
      \ contains=vifmCommand,vifmCmdCommandSt,vifmMarkCommandSt,vifmFtCommandSt,vifmMap,vifmMapSt,vifmExecute,vifmCommands
syntax region vifmCmdCommandSt start='^\s*com\%[mand]' skip='\n\s*\\' end='$' keepend contains=vifmCmdCommand
syntax region vifmMarkCommandSt start='^\s*ma\%[rk]\>' end='$' keepend oneline contains=vifmMarkCommand
syntax region vifmFtCommandSt start='^\s*file[tvx]' end='$' keepend oneline contains=vifmFtCommand
syntax region vifmExecute start='!' end='$' keepend oneline contains=vifmNotation
syntax region vifmCommands start=':' end='$' keepend oneline contains=vifmCommand,vifmNotation,vifmExecute
syntax match vifmMapLhs /\S\+/ contained contains=vifmNotation
syntax region vifmSet matchgroup=vifmCommand start='\<se\%[t]\>' skip='\n\s*\\' end='$' keepend contains=vifmOption,vifmSetString
syntax region vifmSetString contained start=+="+hs=s+1 skip=+\\\\\|\\"+  end=+"+
syntax region vifmSetString contained start=+='+hs=s+1 skip=+\\\\\|\\'+  end=+'+

" Ange-bracket notation
syntax case ignore
syntax match vifmNotation '<\(cr\|space\|f\d\{1,2\}\|c-[a-z[\]^_]\)>'
syntax case match

" Highlight
highlight link vifmComment Comment
highlight link vifmCommand Statement
highlight link vifmCmdCommand Statement
highlight link vifmMarkCommand Statement
highlight link vifmFtCommand Statement
highlight link vifmMap Statement
highlight link vifmOption PreProc
highlight link vifmNotation Special
highlight link vifmSetString String
