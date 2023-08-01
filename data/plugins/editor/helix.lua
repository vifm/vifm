--[[

Wraps helix for handling basic editor use cases within Vifm.

Unimplemented actions:
 * open-help (:help when 'vimhelp' is set):
 * edit-list ("v" key in navigation menus):
   See: https://github.com/helix-editor/helix/discussions/5902

--]]

local M = {}

M.handlers = {}

M.handlers["open-help"] = function(info)
    vifm.errordialog("Unimplemented",
                     "Can't view Vim-style help via Helix.\n"..
                     "Do `:set novimhelp` to make argumentless `:help` work.")
    return false
end

M.handlers["edit-one"] = function(info)
    local posarg = ""
    if info.line ~= nil and info.column == nil then
        posarg = ':' .. info.line
    elseif info.line ~= nil and info.column ~= nil then
        posarg = string.format(":%d:%d", info.line, info.column)
    end

    local exitcode = vifm.run {
        cmd = string.format("hx %s%s", vifm.escape(info.path), posarg),
        usetermmux = not info.mustwait,
    }

    return exitcode == 0
end

function map(table, functor)
    local result = {}
    for key, value in pairs(table) do
        result[key] = functor(value)
    end
    return result
end

M.handlers["edit-many"] = function(info)
    local exitcode = vifm.run {
        cmd = string.format("hx %s",
                            table.concat(map(info.paths, vifm.escape), ' '))
    }

    return exitcode == 0
end

M.handlers["edit-list"] = function(info)
    vifm.errordialog("Unimplemented", "Can't view quickfix via Helix.")
    return false
end

return M
