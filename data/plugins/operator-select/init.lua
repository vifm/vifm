--[[

Adds mappings that select entries in normal mode.

Usage:

    s<selector>, e.g., `sa` selects all entries in a view.
    [count]ss selects [count] entries next to the cursor.

--]]

local M = {}

local added = vifm.keys.add {
    shortcut = 's',
    description = 'select entries',
    modes = { 'normal' },
    followedby = 'selector',
    handler = function(info)
        vifm.currview():select({ indexes = info.indexes })
    end,
}
if not added then
    vifm.sb.error('Failed to register s')
end

local function range(begin, end_)
    local ints = {}
    for i = begin, end_ do
        table.insert(ints, i)
    end
    return ints
end

local added = vifm.keys.add {
    shortcut = 'ss',
    description = 'select [count] entries',
    modes = { 'normal' },
    handler = function(info)
        local count = info.count or 1
        local currview = vifm.currview()
        local indexes = range(currview.currententry,
                              currview.currententry + count - 1)
        currview:select({ indexes = indexes })
    end,
}
if not added then
    vifm.sb.error('Failed to register ss')
end

return M
