--[[

Provides :DevIcons command to manage dev-icons.

Usage examples:

    Toggle dev icons.
    :DevIcons

    Toggle individual category.
    :DevIcons category

--]]

-- TODO: more fine-grained representation and detailed formatting in format()
local icons = {
    types = ' :dir:/, :exe:, :reg:, :link:,| :fifo:,= :sock:',

    -- various file names
    other = ' ::../::, ::*.sh::',
    cxx = ' ::*.hpp,,*.cpp,,*.cc,,*.hh::, ::*.h,,*.c::',
    docs = ' ::copying,,license::, ::.git/,,*.git/::/',
    ebooks = ' ::*.epub,,*.fb2,,*.djvu::, ::*.pdf::',
    xml = ' ::*.htm,,*.html,,*.shtml,,*.xhtml,,*.xml::',

    archives = ' ::*.7z,,*.ace,,*.arj,,*.bz2,,*.cpio,,*.deb,,*.dz,,*.gz,,'..
                   '*.jar,,*.lzh,,*.lzma,,*.rar,,*.rpm,,*.rz,,*.tar,,*.taz,,'..
                   '*.tb2,,*.tbz,,*.tbz2,,*.tgz,,*.tlz,,*.trz,,*.txz,,*.tz,,'..
                   '*.tz2,,*.xz,,*.z,,*.zip,,*.zoo::',
    images = ' ::*.bmp,,*.gif,,*.jpeg,,*.jpg,,*.ico,,*.png,,*.ppm,,*.svg,,'..
                 '*.svgz,,*.tga,,*.tif,,*.tiff,,*.xbm,,*.xcf,,*.xpm,,*.xspf,,'..
                 '*.xwd::',
    audio = ' ::*.aac,,*.anx,,*.asf,,*.au,,*.axa,,*.flac,,*.m2a,,*.m4a,,'..
                '*.mid,,*.midi,,*.mp3,,*.mpc,,*.oga,,*.ogg,,*.ogx,,*.ra,,'..
                '*.ram,,*.rm,,*.spx,,*.wav,,*.wma,,*.ac3::',
    media = ' ::*.avi,,*.ts,,*.axv,,*.divx,,*.m2v,,*.m4p,,*.m4v,,.mka,,'..
                '*.mkv,,*.mov,,*.mp4,,*.flv,,*.mp4v,,*.mpeg,,*.mpg,,*.nuv,,'..
                '*.ogv,,*.pbm,,*.pgm,,*.qt,,*.vob,,*.wmv,,*.xvid,,*.webm::',
    office = ' ::*.doc,,*.docx::, ::*.xls,,*.xlsm,,*.xlsx::,'..
             ' ::*.pptx,,*.ppt::',
}

local active = false
local original_classify

local categories = {}
for key, _ in pairs(icons) do
    categories[key] = true
end

local function format()
    local formatted = ''
    for key, value in pairs(icons) do
        if categories[key] then
            formatted = formatted..value..','
        end
    end
    return formatted
end

local function toggle()
    if active then
        vifm.opts.global.classify = original_classify
    else
        original_classify = vifm.opts.global.classify
        vifm.opts.global.classify = format()
    end
    active = not active
end

local function devicons(info)
    if #info.argv == 0 then
        toggle()
        return
    end

    category = info.argv[1]

    local enabled = categories[category]
    if enabled == nil then
        vifm.sb.error('Unknown category: '..category)
        return
    end

    categories[category] = not enabled

    if active then
        vifm.opts.global.classify = format()
    end
end

local function complete_devicons(info)
    local prefix = info.arg
    local prefix_len = prefix:len()
    local matches = {}
    for key, _ in pairs(categories) do
        if key:sub(0, prefix_len) == prefix then
            matches[#matches + 1] = key
        end
    end
    return { matches = matches }
end

-- this does NOT overwrite pre-existing user command
local added = vifm.cmds.add {
    name = "DevIcons",
    description = "use iconic font to simulate icons",
    handler = devicons,
    complete = complete_devicons,
    maxargs = 1,
}
if not added then
    vifm.sb.error("Failed to register :DevIcons")
end

-- enable icons on startup
toggle()

return {}
