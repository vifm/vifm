--[[

Example of displaying target of symbolic links on statusline.

Usage example for `vifmrc`:

    set statusline=#statusbar#fmt

--]]

local function fmt(info)
    local view = info.view
    local entry = view:entry(view.currententry)
    local format = " %t%=%A %10u:%-7g %15s %20d "
    if entry.type == 'link' then
        local target = entry.gettarget()
        target = string.gsub(target, "%%", "%%%%")
        format = " %t -> " .. target .. " %= %A %10u:%-7g %15s %20d "
    end
    return { format = format }
end

local added = vifm.addhandler {
    name = "fmt",
    handler = fmt,
}
if not added then
    vifm.sb.error("Failed to register #%s#fmt", vifm.plugin.name)
end

return {}
