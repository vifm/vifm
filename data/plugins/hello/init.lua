--[[

Provides :Hello command that accepts one optional argument and greets in return.

Usage examples:

    :Hello
    :Hello me

--]]

local cmd = vifm.plugin.require('command')

local M = {}

local function greet(info)
    vifm.sb.error(string.format('Hello, %s, %s!', info.args, info.argv[1]))
end

-- this does NOT overwrite pre-existing user command
local added = vifm.cmds.add {
    name = "Hello",
    description = "greet",
    handler = cmd.greet,
    maxargs = -1,
}
if not added then
    vifm.sb.error("Failed to register :Hello")
end

return M
