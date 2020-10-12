" Vim script that converts Vim colorschemes to Vifm
" Author:      Roman Plšl
" Maintainer:  xaizek <xaizek@posteo.net>
" Last Change: October 12, 2020

function! s:ConvertGroup(gr, to, deffg, defbg)
	let syn = synIDtrans(hlID(a:gr))
	let fg = synIDattr(syn, "fg")
	let bg = synIDattr(syn, "bg")
	let bold = (synIDattr(syn, "bold") == "1")
	let reverse = (synIDattr(syn, "reverse") == "1")
	let errors = []
	let line = "highlight " . a:to

	if fg[0] == '#' || bg[0] == '#'
		throw VifmTrueColors
	endif

	" handle foreground
	if empty(fg)
		let errors += [
			\'" - incomplete source color scheme: missing fg of ' . a:gr
		\]
		let fg = a:deffg
	endif
	let line .= " ctermfg=" . fg

	" handle background
	if empty(bg)
		let errors += [
			\'" - incomplete source color scheme: missing bg of ' . a:gr
		\]
		let bg = a:defbg
	endif
	let line .= " ctermbg=" . bg

	" handle attributes
	if bold || reverse
		let attrs = []
		if bold
			let attrs += ["bold"]
		endif
		if reverse
			let attrs += ["reverse"]
		endif
		let line .= " cterm=" . join(attrs, ',')
	else
		let line .= " cterm=none"
	endif
	return [errors, line]
endfun

function! s:ConvertCurrentScheme()
	"     Vim group       Vifm group   deffg defbg
	let map = [
		\["Normal",       "Win",         7,   0],
		\["NonText",      "OtherWin",    8,   -1],
		\["AuxWin"],
		\["OddLine"],
		\[],
		\["StatusLineNC", "TopLine",     7,   15],
		\["StatusLine",   "TopLineSel",  7,   0],
		\[],
		\["TabLine",      "TabLine",     0,   7],
		\["TabLineSel",   "TabLineSel",  7,   15],
		\[],
		\["MsgSeparator", "JobLine",     6,   0],
		\["StatusLine",   "StatusLine",  7,   0],
		\["VertSplit",    "Border",      0,   17],
		\[],
		\["Cursor",       "CurrLine",    0,   12],
		\["lCursor",      "OtherLine",   0,   4],
		\["LineNr",       "LineNr",      -1,  -1],
		\[],
		\["Visual",       "Selected",    0,   10],
		\["DiffChange",   "CmpMismatch", 0,   225],
		\[],
		\["Normal",       "SuggestBox",  0,   14],
		\["Pmenu",        "WildMenu",    0,   225],
		\[],
		\["Normal",       "CmdLine",     7,   0],
		\["ErrorMsg",     "ErrorMsg",    15,  1],
		\[],
		\["Keyword",      "Directory",   130, -1],
		\["Macro",        "Executable",  5,   -1],
		\["Debug",        "Socket",      5,   -1],
		\["Delimiter",    "Device",      5,   -1],
		\["String",       "Fifo",        1,   -1],
		\["Number",       "Link",        1,   -1],
		\["Todo",         "BrokenLink",  0,   11],
		\["HardLink"],
		\[],
		\["User1..User9"]
	\]

	let output = ["highlight clear", '']
	let allerrors = []

	for item in map
		if len(item) == 4
			let [errors, line] = s:ConvertGroup(item[0], item[1], item[2],
			                                  \ item[3])
			let allerrors += errors
			let output += [line]
		elseif len(item) == 1
			let output += ['" no conversion defined for ' . item[0]]
		elseif empty(item)
			let output += ['']
		endif
	endfor

	if !empty(allerrors)
		let allerrors = ['', '" warnings:'] + allerrors
	endif

	return output + allerrors
endfun

function! vifm#colorconv#convert(...) abort
	if has('gui_running')
		echoerr 'Should be run in a terminal'
		return
	endif
	if &t_Co != 256
		echoerr 'Should be run in 256-color mode'
		return
	endif

	let cs = g:colors_name

	try
		let schemes = (a:0 > 0 ? a:000 : [g:colors_name])
		for scheme in schemes
			let output = [
				\'" automatically converted from Vim color scheme ' . scheme,
				\''
			\]
			try
				execute "colorscheme" scheme
				let output += s:ConvertCurrentScheme()
				call writefile(output, scheme . ".vifm")
			catch /E185:/ " cannot find color scheme
			catch /VifmTrueColors/ " cannot find color scheme
				echoerr "Can't convert ".scheme." while 'termguicolors' is on"
				return
			endtry
		endfor
	finally
		execute "colorscheme" cs
	endtry
endfunction

" vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab :
