--[[

Provides Lua handler that wraps invocation of an editor.

Usage example:

    set vicmd=#editor#run

--]]

local gvim = vifm.plugin.require('gvim')

local function run(info)
    local handler = gvim.handlers[info.action]
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

return {}
