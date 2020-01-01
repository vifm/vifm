" Vim script that converts Vim colorschemes to vifm
"
" Usage:
"  1. Run vim in 256 color mode (not gui, not 16 color mode)
"  2. Set output folder as your current working directory (:cd target)
"  3. Edit the list of schemes you want to convert below
"  4. Run :source colorconv.vim
"  5. Don't look at the screen if you're epilleptic

let schemes = "torte mustang simple_b greenvision lilydjwg_green rakr-light Chasing_Logic tchaba CodeFactoryv3 tango2 carvedwood manxome sandydune twilight256 lightning greens blaquemagick stingray aurora evening1 tetragrammaton feral detailed anotherdark jellybeans bvemu smyck duotone-darkforest xoria256 darkdot burnttoast256 desert256v2 sunburst elise inkpot tango-morning midnight eva jhlight duotone-darkpark BlackSea turbo thestars hhdgreen base16-railscasts zephyr lumberjack antares adobe soso triplejelly seoul herokudoc duotone-darkspace leglight2 wombat256 colorsbox-faff pt_black getafe mayansmoke forneus hhdgray nightshade_print kyle 256-jungle asmdev bluedrake morning derefined simple256 greyblue flattr crayon native mango wolfpack moonshine fu phphaxor golden blue pink asmdev2 colorsbox-steighties charged-256 impact github gentooish literal_tango busybee jhdark guardian Benokai leo gothic darth nightwish softbluev2 apprentice soda phpx grb256 anderson mizore koehler gemcolors goodwolf 256-grayvim duotone-darkheath materialbox stonewashed-256 inori 0x7A69_dark gruvbox sprinkles holokai cobalt2 wargrey darkburn elda coldgreen seashell redstring beauty256 maroloccio symfony playroom desertink colorzone base16-atelierlakeside brogrammer up calmar256-light bubblegum-256-dark base16-atelierestuary lilydjwg_dark colorsbox-material Tomorrow-Night-Blue campfire bubblegum-256-light peachpuff miko murphy bayQua landscape perfect flatcolor bluegreen duotone-darkearth smp vj base16-ateliersavanna kalt beekai darkblack mars duotone-darksea darkZ yeller xterm16 coda vibrantink tabula kaltex thornbird pf_earth obsidian custom scheakur tango-desert blueprint montz hhdblue moria textmate16 mrkn256 jiks lapis256 base16-ateliersulphurpool reloaded rdark-terminal duotone-darkpool blackbeauty simplewhite duotone-darkdesert badwolf tomatosoup grishin mopkai bog adrian borland 1989 tir_black heroku spacegray oceandeep redblack coffee simpleandfriendly genericdc-light PaperColor minimalist doorhinge hhdred industrial fog seattle tchaba2 tibet benlight base16-atelierheath duotone-darkmeadow distinguished tropikos astronaut newspaper itg_flat genericdc ego icansee evolution neverness earendel jellyx cake synic devbox-dark-256 iangenzo baycomb kkruby vcbc laederon colorer asmanian2 autumnleaf predawn made_of_code lxvc monoacc desert af prmths material ir_black watermark enigma mushroom base16-atelierplateau mod8 calmar256-dark SlateDark candycode cabin swamplight blazer vividchalk marklar ps_color pablo black_angus sole duotone-darklake VIvid ibmedit luinnar chlordane Tomorrow-Night strange freya Tomorrow-Night-Bright skittles_dark mojave industry wombat256i bmichaelsen grey2 seti phoenix eclipse transparent mac_classic moss railscasts blacklight chocolate white darkdevel woju zen hemisu tangoshady legiblelight basic disciple slate2 desertEx peaksea SweetCandy dual znake frood hornet hhdyellow valloric lilypink sourcerer materialtheme heroku-terminal nightflight saturn sorcerer neonwave pride denim lingodirector ChocolatePapaya lettuce desert256 scooby Tomorrow-Night-Eighties kalahari nightsky leya brown blackboard shobogenzo rcg_term far base16-atelierseaside kalisi aiseered bocau bw zenburn sadek1 rastafari autumn muon trivial256 sean busierbee neverland-darker duotone-darkcave harlequin colorsbox-stbright skittles_berry print_bw bubblegum kellys fruidle shine delek buddy ecostation wuye material-theme dull dracula nazca vimbrant lucid babymate256 adaryn flattened_light c madeofcode crt clue pencil seoul256 base16-ateliercave carvedwoodcool alduin navajo candyman lizard256 deepsea automation Monokai neverland2-darker jammy elrond dante meta5 hhdcyan sift colorsbox-stblue colorful sweater contrasty ingretu janah desertedoceanburnt summerfruit256 jelleybeans thor penultimate flatlandia gardener wasabi256 bensday oxeded d8g_01 pw nature PapayaWhip elisex colorsbox-greenish wellsokai monokain impactjs eva01 impactG tango CandyPaper sol-term shades-of-teal cool gryffin less nuvola tayra bluez Dev_Delight donbass OceanicNext d8g_03 relaxedgreen iceberg termschool base sierra Revolution northsky molokai sky habiLight d8g_02 flattown desertedocean flattened_dark twitchy maui base16-atelierforest darkzen void hhdmagenta colorsbox-stnight louver erez asu1dark newsprint pleasant neverland hybrid soruby ChocolateLiquor potts zellner felipec django Tomorrow molokai_dark late_evening luna-term wombat256mod ansi_blows enzyme ubaryd nightshade satori birds-of-paradise smpl codeschool southernlights elflord evening moonshine_lowcontrast flatui navajo-night ubloh neverland2 oceanblack256 understated nightflight2 cake16 kiss darkblue kruby behelit base16-atelierdune hotpot duotone-dark herald monochrome C64 martin_krischik graywh radicalgoodspeed colorful256 monokai-chris lucius lodestone d8g_04 kolor flatland 256_noir developer motus blink khaki"

function! s:onegroup(gr, to, defg, debg)
	let syn = synIDtrans(hlID(a:gr))
	let fg = synIDattr(syn, "fg")
	let bg = synIDattr(syn, "bg")
	let bold = (synIDattr(syn, "bold") == "1")
	let reverse = (synIDattr(syn, "reverse") == "1")
	let result = "highlight ".a:to

	" handle foreground
	if fg == ""
		call append(0, "\" incomplete color scheme, missing fg " . a:gr)
		let fg = a:defg
	endif
	let result = result . " ctermfg=".fg 

	" handle background
	if bg == ""
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

function! s:ConvertOne()
	let result = ["highlight clear"]
	let result += s:onegroup("Normal", "Win", 7, 0)
	let result += s:onegroup("NonText", "OtherWin", 8, 0)
	let result += s:onegroup("VertSplit", "Border", 0, 17)
	let result += s:onegroup("TabLine", "TabLine", 0, 7)
	let result += s:onegroup("TabLineSel", "TabLineSel", 7, 15)
	let result += s:onegroup("StatusLineNC", "TopLine", 7, 15)
	let result += s:onegroup("StatusLine", "TopLineSel", 7, 0)
	let result += s:onegroup("Normal", "CmdLine", 7, 0)
	let result += s:onegroup("ErrorMsg", "ErrorMsg", 15, 1)
	let result += s:onegroup("StatusLine", "StatusLine", 7, 0)
	let result += s:onegroup("MsgSeparator", "JobLine", 6, 0)
	let result += s:onegroup("Pmenu", "WildMenu", 0, 225)
	let result += s:onegroup("Normal", "SuggestBox", 0, 14)
	let result += s:onegroup("Cursor", "CurrLine", 0, 12)
	let result += s:onegroup("lCursor", "OtherLine", 0, 4)
	let result += s:onegroup("Visual", "Selected", 0, 10)
	let result += s:onegroup("Keyword", "Directory", 130, 0)
	let result += s:onegroup("Number", "Link", 1, 0)
	let result += s:onegroup("Todo", "BrokenLink", 0, 11)
	let result += s:onegroup("Debug", "Socket", 5, 0)
	let result += s:onegroup("Delimiter", "Device", 5, 0)
	let result += s:onegroup("Macro", "Executable", 5, 0)
	let result += s:onegroup("String", "Fifo", 1, 0)
	let result += s:onegroup("DiffChange", "CmpMismatch", 0, 225)

	call append(0, result)
endfun


for it in split(schemes)
	enew
	call append(0, "\" converted from Vim color scheme ". it)
	exec "color " it
	call s:ConvertOne()
	exec "write! " it . ".vifm"
endfor

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
