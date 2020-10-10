--[[

Provides :Hello command that accepts one argument that specifies name and greets
you by that name as a test of a :command.

Usage example:

    :Hello me

--]]

local M = {}

local function greet(info)
    vifm.sb.error(string.format('Hello, %s!', info.args))
end

-- this does NOT overwrite pre-existing user command
local added = vifm.addcommand {
    name = "Hello",
    descr = "greet you",
    handler = greet,
    minargs = 1,
}
if not added then
    vifm.sb.error("Failed to register :Hello")
end

return M
