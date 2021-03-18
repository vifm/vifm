--[[

Provides three user-defined view column types:
 * LsSize -- ls-like size column (at most 4 characters in width)
 * LsTime -- ls-like (also MC-like) time where time is shown if year matches
             current, otherwise year is displayed instead of time
 * MCSize -- MC-like size column (at most 7 characters in width)

Usage example:

    :set viewcolumns+=8{MCSize}

--]]

local function lsSize(info)
    local unit = 1
    local size = info.entry.size
    local str
    while true do
        str = string.format('%3.1f', size)
        if str:sub(-2) == '.0' then
            str = str:sub(1, -3)
        end
        if str:len() <= 3 then
            break
        end

        size = size/1000
        if math.floor(size) == 0 then
            str = str:sub(1, -3)
            break
        end

        unit = unit + 1
    end

    return str..string.sub('BKMGTPEZY', unit, unit)
end

local function mcSize(info)
    local maxLen = 7
    local unit = 0
    local size = info.entry.size
    local str
    while true do
        str = tostring(size)..string.sub('KMGTPEZY', unit, unit)
        if str:len() <= maxLen then
            return str
        end

        size = size//1000
        unit = unit + 1
    end
end

local currentYear<const> = os.date('*t').year
local function lsTime(info)
    local time = info.entry.mtime
    local year = os.date('*t', time).year
    if year == currentYear then
        return os.date('%b %d %H:%M', time)
    else
        return os.date('%b %d  %Y', time)
    end
end

local added = vifm.addcolumntype {
    name = 'LsSize',
    handler = lsSize
}
if not added then
    vifm.sb.error("Failed to add LsSize view column")
end

local added = vifm.addcolumntype {
    name = 'LsTime',
    handler = lsTime
}
if not added then
    vifm.sb.error("Failed to add LsTime view column")
end

local added = vifm.addcolumntype {
    name = 'MCSize',
    handler = mcSize
}
if not added then
    vifm.sb.error("Failed to add MCSize view column")
end

return {}
