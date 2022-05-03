--[[

Wraps gvim for handling all editor use cases within Vifm.

--]]

local M = {}

M.handlers = {}

M.handlers["open-help"] = function(info)
    vifm.startjob {
        cmd = string.format("gvim %s %s +only",
                            vifm.escape("+set runtimepath+=" .. info.vimdocdir),
                            vifm.escape("+help " .. info.topic))
    }

    return true
end

M.handlers["edit-one"] = function(info)
    local forkarg = info.mustwait and "-f" or ""

    local posarg = ""
    if info.line ~= nil and info.column == nil then
        posarg = '+' .. info.line
    elseif info.line ~= nil and info.column ~= nil then
        posarg = string.format("+call cursor(%d, %d)", info.line, info.column)
    end

    local job = vifm.startjob {
        cmd = string.format("gvim %s %s %s", forkarg, vifm.escape(posarg),
                            vifm.escape(info.path))
    }

    if not info.mustwait then
        return true
    end

    return job:exitcode() == 0
end

function map(table, functor)
    local result = {}
    for key, value in pairs(table) do
        result[key] = functor(value)
    end
    return result
end

M.handlers["edit-many"] = function(info)
    vifm.startjob {
        cmd = string.format("gvim %s",
                            table.concat(map(info.paths, vifm.escape), ' '))
    }

    return true
end

M.handlers["edit-list"] = function(info)
    local cmd
    if info.isquickfix then
        cmd = string.format("gvim +cgetbuffer +bd! +cc%d -", info.current)
    else
        local exec = "exe 'bd!|args' join(map(getline('1','$'),'fnameescape(v:val)'))"
        cmd = string.format("gvim +%s +argument%d -",
                            vifm.escape(exec),
                            info.current)
    end

    local job = vifm.startjob {
        cmd = cmd,
        iomode = 'w'
    }

    local input = table.concat(info.entries, '\n')

    local pipe = job:stdin()
    pipe:write(input)
    pipe:close()

    return true
end

return M
