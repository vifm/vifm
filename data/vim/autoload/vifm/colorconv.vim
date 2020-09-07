" Vim script that converts Vim colorschemes to Vifm
" Author:      Roman Plšl
" Maintainer:  xaizek <xaizek@posteo.net>
" Last Change: September 7, 2020

function! s:ConvertGroup(gr, to, defg, debg)
	let syn = synIDtrans(hlID(a:gr))
	let fg = synIDattr(syn, "fg")
	let bg = synIDattr(syn, "bg")
	let bold = (synIDattr(syn, "bold") == "1")
	let reverse = (synIDattr(syn, "reverse") == "1")
	let result = "highlight ".a:to

	" handle foreground
	if empty(fg)
		call append(0, "\" incomplete color scheme, missing fg " . a:gr)
		let fg = a:defg
	endif
	let result = result . " ctermfg=".fg

	" handle background
	if empty(bg)
		call append(0, "\" incomplete color scheme, missing bg " . a:gr)
		let bg = a:debg
	endif
	let result = result." ctermbg=".bg

	" handle attributes
	if bold || reverse
		let attrs = []
		if bold
			let attrs += ["bold"]
		endif
		if reverse
			let attrs += ["reverse"]
		endif
		let result .= " cterm=".join(attrs, ',')
	else
		let result .= " cterm=none"
	endif
	return [result]
endfun

function! s:ConvertCurrentScheme()
	let result = ["highlight clear"]
	let result += s:ConvertGroup("Normal", "Win", 7, 0)
	let result += s:ConvertGroup("NonText", "OtherWin", 8, 0)
	let result += s:ConvertGroup("VertSplit", "Border", 0, 17)
	let result += s:ConvertGroup("TabLine", "TabLine", 0, 7)
	let result += s:ConvertGroup("TabLineSel", "TabLineSel", 7, 15)
	let result += s:ConvertGroup("StatusLineNC", "TopLine", 7, 15)
	let result += s:ConvertGroup("StatusLine", "TopLineSel", 7, 0)
	let result += s:ConvertGroup("Normal", "CmdLine", 7, 0)
	let result += s:ConvertGroup("ErrorMsg", "ErrorMsg", 15, 1)
	let result += s:ConvertGroup("StatusLine", "StatusLine", 7, 0)
	let result += s:ConvertGroup("MsgSeparator", "JobLine", 6, 0)
	let result += s:ConvertGroup("Pmenu", "WildMenu", 0, 225)
	let result += s:ConvertGroup("Normal", "SuggestBox", 0, 14)
	let result += s:ConvertGroup("Cursor", "CurrLine", 0, 12)
	let result += s:ConvertGroup("lCursor", "OtherLine", 0, 4)
	let result += s:ConvertGroup("Visual", "Selected", 0, 10)
	let result += s:ConvertGroup("Keyword", "Directory", 130, 0)
	let result += s:ConvertGroup("Number", "Link", 1, 0)
	let result += s:ConvertGroup("Todo", "BrokenLink", 0, 11)
	let result += s:ConvertGroup("Debug", "Socket", 5, 0)
	let result += s:ConvertGroup("Delimiter", "Device", 5, 0)
	let result += s:ConvertGroup("Macro", "Executable", 5, 0)
	let result += s:ConvertGroup("String", "Fifo", 1, 0)
	let result += s:ConvertGroup("DiffChange", "CmpMismatch", 0, 225)

	call append(0, result)
endfun

function! vifm#colorconv#convert(...) abort
	if has('gui_running')
		echoerr 'Should be run in a terminal'
	endif
	if &t_Co != 256
		echoerr 'Should be run in 256-color mode'
	endif

	let schemes = (a:0 > 0 ? a:000 : [g:colors_name])
	for scheme in schemes
		enew
		call append(0, "\" converted from Vim color scheme ". scheme)
		exec "color " scheme
		call s:ConvertCurrentScheme()
		exec "write! " scheme . ".vifm"
	endfor
endfunction

"- TabLine - tab line color (for vifm-'tabscope' set to "global")
"- TabLineSel - color of the tip of selected tab (regardless of
"               vifm-'tabscope')
"- TopLineSel - top line color of the current pane
"- TopLine - top line color of the other pane
"- CmdLine - the command line/status bar color
"- ErrorMsg - color of error messages in the status bar
"- StatusLine - color of the line above the status bar
"- JobLine - color of job line that appears above the status line
"- WildMenu - color of the wild menu items
"- SuggestBox - color of key suggestion box
"- CurrLine - line at cursor position in active view
"- OtherLine - line at cursor position in inactive view
"- Selected - color of selected files
"- Directory - color of directories
"- Link - color of symbolic links in the views
"- BrokenLink - color of broken symbolic links
"- Socket - color of sockets
"- Device - color of block and character devices
"- Executable - color of executable files
"- Fifo - color of fifo pipes
"- CmpMismatch - color of mismatched files in side-by-side comparison by paths
