--[[

Wraps vim for handling all editor use cases within Vifm.

Difference from builtin functionality: opening lists (v in menus) will use a
separate tab in terminal multiplexer at the risk of leaving some temporary files
around if an error occurs.

--]]

local M = {}

M.handlers = {}

M.handlers["open-help"] = function(info)
    local exitcode = vifm.run {
        cmd = string.format("vim %s %s +only",
                            vifm.escape("+set runtimepath+=" .. info.vimdocdir),
                            vifm.escape("+help " .. info.topic))
    }

    return exitcode == 0
end

M.handlers["edit-one"] = function(info)
    local posarg = ""
    if info.line ~= nil and info.column == nil then
        posarg = '+' .. info.line
    elseif info.line ~= nil and info.column ~= nil then
        posarg = string.format("+call cursor(%d, %d)", info.line, info.column)
    end

    local exitcode = vifm.run {
        cmd = string.format("vim %s %s", vifm.escape(posarg),
                            vifm.escape(info.path)),
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
        cmd = string.format("vim %s",
                            table.concat(map(info.paths, vifm.escape), ' '))
    }

    return exitcode == 0
end

M.handlers["edit-list"] = function(info)
    local input = table.concat(info.entries, '\n')

    local tmp = os.tmpname()
    local f = io.open(tmp, 'w')
    f:write(input)
    f:close()

    local cwd = vifm.currview().cwd

    local cmd
    if info.isquickfix then
        cmd = string.format("vim +%s +cgetbuffer +%s +bd! +cc%d %s",
                            vifm.escape('cd ' .. cwd),
                            vifm.escape('silent! !rm %'),
                            info.current,
                            vifm.escape(tmp))
    else
        local exec = "exe 'bd!|args' join(map(getline('1','$'),'fnameescape(v:val)'))"
        cmd = string.format("vim +%s +%s +%s +argument%d %s",
                            vifm.escape('cd ' .. cwd),
                            vifm.escape('silent! !rm %'),
                            vifm.escape(exec),
                            info.current,
                            vifm.escape(tmp))
    end

    local exitcode = vifm.run { cmd = cmd }
    return exitcode == 0
end

return M
