--[[

Provides Lua handler that wraps invocation of an editor.  Name of the wrapper
needs to be passed in as an argument.

Builtin wrappers:
 * gvim
 * vim

Usage example:

    set vicmd='#editor#run gvim'

--]]

local impls = {
    vim = vifm.plugin.require('vim'),
    gvim = vifm.plugin.require('gvim'),
}

local function run(info)
    local args = string.gmatch(info.command, "(%S+)")
    local _ = args()
    local editor = args()
    if editor == nil then
        vifm.errordialog(vifm.plugin.name, "Editor name is missing")
        return { success = 1 }
    end

    local impl = impls[editor]
    if impl == nil then
        vifm.errordialog(vifm.plugin.name, "Editor name is wrong: " .. editor)
        return { success = 1 }
    end

    local handler = impl.handlers[info.action]
    if handler == nil then
        vifm.errordialog(vifm.plugin.name, "Unexpected action: " .. info.action)
        return { success = false }
    end
    return { success = handler(info) }
end

local added = vifm.addhandler {
    name = "run",
    handler = run,
}
if not added then
    vifm.sb.error("Failed to register #run")
end

return { wrappers = impls }
