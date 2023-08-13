--[[

Puts cursor on a recently modified/changed/accessed entry in current view,
which frees from the need to change and restore 'sort' to do it otherwise.

Adds the following mappings:

* [count]rm -- puts cursor on a [count]'s recently modified entry
* [count]rc -- puts cursor on a [count]'s recently changed entry
* [count]ra -- puts cursor on a [count]'s recently accessed entry

--]]

local M = {}

local function make_entries_sorter(attr)
    -- sorts entries by `attr` and entry name in descending order
    return function(e1, e2)
        -- like -attr in 'sort'
        if e1[attr] ~= e2[attr] then
            return e1[attr] > e2[attr]
        -- like +dir in 'sort'
        elseif (e1.isdir and not e2.isdir) or (not e1.isdir and e2.isdir) then
            return e1.isdir
        -- like +name in 'sort'
        else
            return e1.name < e2.name
        end
    end
end

local function full_path(e)
    return e.location .. '/' .. e.name
end

local function recent_entry_index(attr, num)
    local currview = vifm.currview()
    local curr_entry = currview.cursor:entry()
    local curr_loc = curr_entry.location
    local entries = {}
    local entry_pos = {}

    local function remember_entry_and_position(e, pos)
        table.insert(entries, e)
        entry_pos[full_path(e)] = pos
    end

    if currview.custom == nil or currview.custom.type ~= 'tree' then
        for i = 1, currview.entrycount do
            local e = currview:entry(i)
            remember_entry_and_position(e, i)
        end
    else
        -- limit to location of current entry in tree-view

        remember_entry_and_position(curr_entry, currview.cursor.pos)

        local function belongs_to_curr_entry_location(i)
            local e = currview:entry(i)
            if e.location:sub(1, curr_loc:len()) ~= curr_loc then
                return false
            end
            if e.location == curr_loc then
                remember_entry_and_position(e, i)
            end
            return true
        end

        -- gather entries around the cursor
        local i = currview.cursor.pos - 1
        while i >= 1 and belongs_to_curr_entry_location(i) do
            i = i - 1
        end

        local i = currview.cursor.pos + 1
        while i <= currview.entrycount and belongs_to_curr_entry_location(i) do
            i = i + 1
        end
    end

    table.sort(entries, make_entries_sorter(attr))

    local num = math.max(num or 1, 1)
    local idx = math.min(num, #entries)
    return entry_pos[full_path(entries[idx])]
end

local function make_handler(attr)
    return function(info)
        vifm.currview().cursor.pos = recent_entry_index(attr, info.count)
    end
end

local function make_handler_selector(attr)
    return function(info)
        local currview = vifm.currview()

        local begin = recent_entry_index(attr, info.count)
        local end_ = currview.cursor.pos
        if begin > end_ then
            begin, end_ = end_, begin
        end

        local indexes = {}
        for i = begin, end_ do
            table.insert(indexes, i)
        end

        return {
            indexes = indexes,
            cursorpos = indexes[1],
        }
    end
end

local function add_key(key, attr, desc)
    local added = vifm.keys.add {
        shortcut = key,
        description = 'move cursor to recently ' .. desc .. ' entry',
        modes = { 'normal', 'visual' },
        handler = make_handler(attr),
    }
    if not added then
        vifm.sb.error('Failed to register ' .. key)
    end

    local added = vifm.keys.add {
        shortcut = key,
        description = 'move cursor to recently ' .. desc .. ' entry',
        modes = { 'normal' },
        isselector = true,
        handler = make_handler_selector(attr),
    }
    if not added then
        vifm.sb.error('Failed to register selector ' .. key)
    end
end

add_key('rm', 'mtime', 'modified')
add_key('rc', 'ctime', 'changed')
add_key('ra', 'atime', 'accessed')

return M
