--[[

Enables use of Ueberzug for viewing images.

Usage example:

    :fileviewer {*.png}
              \ #ueberzug#view %px %py %pw %ph
              \ %pc
              \ #ueberzug#clear

Ueberzug: https://github.com/seebye/ueberzug/

--]]

-- TODO: check that `ueberzug` is available
-- TODO: generate JSON properly
-- TODO: handle other types of files
-- TODO: launch Ueberzug on first use

local M = { }

local uberzug = vifm.startjob {
    cmd = 'ueberzug layer --silent',
    iomode = 'w'
}

local pipe = uberzug:stdin()
local layer_id = "vifm-preview"

local function view(info)
    local format = '{ "action":"add", "identifier":"%s",'
                 ..'  "x":%d, "y":%d, "width":%d, "height":%d,'
                 ..'  "path":"%s"'
                 ..'}\n'

    local message = format:format(layer_id,
                                  info.x, info.y, info.width, info.height,
                                  info.path)
    print(message)
    pipe:write(message)
    pipe:flush()
    return { lines = {} }
end

local function clear(info)
    local format = '{"action":"remove", "identifier":"%s"}\n'

    local message = format:format(layer_id)
    print(message)
    pipe:write(message)
    pipe:flush()
    return { lines = {} }
end

local added = vifm.addhandler {
    name = "view",
    handler = view,
}
if not added then
    vifm.sb.error("Failed to register #view")
end

local added = vifm.addhandler {
    name = "clear",
    handler = clear,
}
if not added then
    vifm.sb.error("Failed to register #clear")
end

return M
