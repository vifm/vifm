--[[

Provides six user-defined view column types:
 * LsSize   -- ls-like size column (at most 4 characters in width)
 * LsTime   -- ls-like (also MC-like) time where time is shown if year matches
               current, otherwise year is displayed instead of time
 * MCSize   -- MC-like size column (at most 7 characters in width)
 * AgeAtime -- integer age based on the atime stat (11 characters in width)
 * AgeCtime -- integer age based on the ctime stat (11 characters in width)
 * AgeMtime -- integer age based on the mtime stat (11 characters in width)

Usage example:

    :set viewcolumns=-{name},8{MCSize}

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

local time_units = {
    {name = 'year',   seconds = 365 * 24 * 60 * 60},
    {name = 'month',  seconds =  30 * 24 * 60 * 60},
    {name = 'week',   seconds =   7 * 24 * 60 * 60},
    {name = 'day',    seconds =       24 * 60 * 60},
    {name = 'hour',   seconds =            60 * 60},
    {name = 'minute', seconds =                 60},
    {name = 'second', seconds =                  1},
}

local function get_age(seconds)
    local diff = os.difftime(os.time(), seconds)
    local future_sign = diff >= 0 and '' or '-'
    diff = math.abs(diff)
    for _, time_unit in ipairs(time_units) do
        if diff >= time_unit.seconds then
            local int = math.floor(diff / time_unit.seconds)
            return string.format('%3s %-7s',
                future_sign .. tostring(int),
                time_unit.name .. (int > 1 and 's' or ' '))
        end
    end
    return '  0 seconds'
end

for _, stat_name in ipairs({'atime', 'ctime', 'mtime'}) do
    local column_name = 'Age' .. stat_name:sub(1, 1):upper() .. stat_name:sub(2)
    local added = vifm.addcolumntype {
        name = column_name,
        handler = function(info)
            return get_age(info.entry[stat_name])
        end
    }
    if not added then
        vifm.sb.error("Failed to add " .. column_name .. " view column")
    end
end

return {}
