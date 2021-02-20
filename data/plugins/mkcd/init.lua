--[[

Provides :MkCd command that creates some path and enters it reporting if
something went wrong.

Usage example:

    :MkCd dir/sub

--]]

local function mkcd(info)
    local path = info.argv[1]

    if vifm.exists(path) then
        vifm.errordialog("MkCd", "Path already exists: "..path)
        return
    end

    if not vifm.makepath(path) then
        vifm.errordialog("MkCd", "Failed to create path: "..path)
        return
    end

    if not vifm.currview():cd(path) then
        vifm.errordialog("MkCd", "Failed to navigate to: "..path)
    end
end

-- this does NOT overwrite pre-existing user command
local added = vifm.cmds.add {
    name = "MkCd",
    description = "create path and enter it",
    handler = mkcd,
    maxargs = 1,
}
if not added then
    vifm.sb.error("Failed to register :MkCd")
end

return {}
