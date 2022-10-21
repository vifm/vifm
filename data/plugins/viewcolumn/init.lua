--[[

Provides the following user-defined view column types:
 * NameLink -- Name column that shows target of symbolic links
 * MimeType -- column that shows MIME type of an entry
 * LsSize   -- ls-like size column (at most 4 characters in width)
 * LsTime   -- ls-like (also MC-like) time where time is shown if year matches
               current, otherwise year is displayed instead of time
 * MCSize   -- MC-like size column
 * AgeAtime -- relative age based on the atime stat (11 characters in width)
 * AgeCtime -- relative age based on the ctime stat (11 characters in width)
 * AgeMtime -- relative age based on the mtime stat (11 characters in width)

 * FileMtime -- modification time for files only

Usage example:

    :set viewcolumns=-{NameLink},8.7{MCSize}

--]]

local function nameLink(info)
    local e = info.entry
    local text = e.classify.prefix .. e.name .. e.classify.suffix
    if e.type == 'link' then
        text = text .. ' -> ' .. e.gettarget()
    end

    if not e.match then
        return { text = text }
    end

    local offset = e.classify.prefix:len()
    return {
        text = text,
        matchstart = offset + e.matchstart,
        matchend = offset + e.matchend
    }
end

local function mimeType(info)
    return {
        text = '<' .. (info.entry:mimetype() or 'UNAVAILABLE') .. '>'
    }
end

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

    return { text = str..string.sub('BKMGTPEZY', unit, unit) }
end

local function mcSize(info)
    local maxLen = info.width
    local unit = 0
    local size = info.entry.size
    local str
    while true do
        str = tostring(size)..string.sub('KMGTPEZY', unit, unit)
        if str:len() <= maxLen then
            return { text = str }
        end

        size = size//1000
        unit = unit + 1
    end
end

local function fileMtime(info)
    local text = ''
    if not info.entry.isdir then
        text = os.date(vifm.opts.global.timefmt, info.entry.mtime)
    end
    return { text = text }
end

local secsPerYear<const> = 365*24*60*60
local function lsTime(info)
    local time = info.entry.mtime
    local ageYears = os.difftime(os.time(), time)//secsPerYear
    if ageYears == 0 then
        return { text = os.date('%b %d %H:%M', time) }
    else
        return { text = os.date('%b %d  %Y', time) }
    end
end

local added = vifm.addcolumntype {
    name = 'NameLink',
    handler = nameLink,
    isprimary = true
}
if not added then
    vifm.sb.error("Failed to add NameLink view column")
end

local added = vifm.addcolumntype {
    name = 'MimeType',
    handler = mimeType
}
if not added then
    vifm.sb.error("Failed to add MimeType view column")
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

local added = vifm.addcolumntype {
    name = 'FileMtime',
    handler = fileMtime
}
if not added then
    vifm.sb.error("Failed to add FileMtime view column")
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
            text = string.format('%3s %-7s',
                future_sign .. tostring(int),
                time_unit.name .. (int > 1 and 's' or ' '))
            return { text = text }
        end
    end
    return { text = '  0 seconds' }
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
