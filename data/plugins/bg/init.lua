--[[

Provides :Bg command that runs external command in background displaying it on
job bar until its done.

Usage example:

    :Bg sleep 10

--]]

local function bg(info)
    local job = vifm.startjob {
        cmd = info.args,
        description = info.args,
        visible = true
    }
end

-- this does NOT overwrite pre-existing user command
local added = vifm.cmds.add {
    name = "Bg",
    description = "run an external command in background",
    handler = bg,
    minargs = 1,
    maxargs = -1,
}
if not added then
    vifm.sb.error("Failed to register :Bg")
end

return {}
