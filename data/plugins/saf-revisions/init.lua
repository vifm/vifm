--[[

    saf-revisions Vifm plugin is providing commands that use

    https://github.com/dusanx/saf

    to provide time-machine like restore functionality.

    Provides
    :SafRevisionsToggle     -- Activate or deactivate saf-revisions
    :SafRevisionsActivate   -- Turn saf-revisions on
    :SafRevisionsDeactivate -- Turn saf-revisions off
    :SafPreviousRevision    -- Starfield! Warp! Magic! Back in time.
    :SafNextRevision        -- Starfield! Warp! Magic! Forward in time.

--]]

local active = false
local curr_path = nil
local other_path = nil
local revisions = {}
local revisions_pos = nil

local function show_revision()
    if revisions_pos == nil then return end
    local path = revisions[revisions_pos]
    if not vifm.currview():cd(path) then
        vifm.sb.error("Failed to navigate to: "..path..", possibly removed by prune.")
        return
    end
    vifm.sb.info("Saf revision: "..revisions_pos.." of "..#revisions.." in "..curr_path)
end

local function turn_on()
    if active then return end
    -- fetch revisions "saf revisions -q /path"
    revisions = {}
    vifm.sb.info("On big folders, saf revisions can take time, please wait...")
    local path = vifm.currview().cwd
    local job = vifm.startjob { cmd = 'saf revisions -q "'..path..'"'}
    for line in job:stdout():lines() do
        vifm.sb.info("On big folders, saf revisions can take time, please wait... "..line)
        table.insert(revisions, line)
    end
    job:wait()
    -- did we fetch anything?
    if #revisions <= 0 then
        vifm.sb.error("Got Nothing! Is saf in path? Is this dir in backup? Maybe this dir didn't change at all?")
        revisions = {}
        return
    end
    -- remote dir check, we can't handle remote
    revisions_pos = 1
    check = revisions[revisions_pos]
    if string.find(check, ":") ~= nil then
        vifm.sb.error("Looks like remote backup, we can't handle that.")
        revisions = {}
        return
    end
    -- we have the results
    active = true
    curr_path = vifm.currview().cwd
    other_path = vifm.otherview().cwd
    if not vifm.otherview():cd(path) then
        vifm.errordialog("saf", "Failed to navigate to: "..path)
    end
    show_revision()
end

local function turn_off()
    if not active then return end
    if not vifm.currview():cd(curr_path) then
        vifm.errordialog("saf", "Failed to navigate to: "..curr_path)
    end
    if not vifm.otherview():cd(other_path) then
        vifm.errordialog("saf", "Failed to navigate to: "..other_path)
    end
    active = false
    curr_path = nil
    other_path = nil
    revisions = {}
    revisions_pos = nil
end

local function SafRevisionsToggle(info)
    if not active then
        turn_on()
    else
        turn_off()
    end
end

local function SafRevisionsActivate(info)
    turn_on()
end

local function SafRevisionsDeactivate(info)
    turn_off()
end

local function SafPreviousRevision(info)
    if revisions_pos == nil then return end
    if revisions_pos < #revisions then revisions_pos = revisions_pos + 1 end
    show_revision()
end

local function SafNextRevision(info)
    if revisions_pos == nil then return end
    if revisions_pos > 1 then revisions_pos = revisions_pos - 1 end
    show_revision()
end

-- Add all commands

if not vifm.cmds.add {
    name = "SafRevisionsToggle",
    description = "turn on or off saf revisions",
    handler = SafRevisionsToggle
} then
    vifm.sb.error("Failed to register :SafRevisionsToggle")
end

if not vifm.cmds.add {
    name = "SafRevisionsActivate",
    description = "turn on saf revisions",
    handler = SafRevisionsActivate
} then
    vifm.sb.error("Failed to register :SafRevisionsActivate")
end

if not vifm.cmds.add {
    name = "SafRevisionsDeactivate",
    description = "turn off saf revisions",
    handler = SafRevisionsDeactivate
} then
    vifm.sb.error("Failed to register :SafRevisionsDeactivate")
end

if not vifm.cmds.add {
    name = "SafPreviousRevision",
    description = "navigate to previous revision",
    handler = SafPreviousRevision
} then
    vifm.sb.error("Failed to register :SafPreviousRevision")
end

if not vifm.cmds.add {
    name = "SafNextRevision",
    description = "navigate to next revision",
    handler = SafNextRevision
} then
    vifm.sb.error("Failed to register :SafNextRevision")
end

return {}
