--[[

Example of a custom powerline-like tab line.

Usage example for `vifmrc`:

    set tabline=#tabline#fmt

User colors for a color scheme:

    " colors:
    " - active tab bg   = 231
    " - active tab fg   = 232
    " - inactive tab bg = 237
    " - inactive tab fg = 7
    hi User11  ctermfg=232  ctermbg=231  cterm=bold " active start
    hi User12  ctermfg=231  ctermbg=237  cterm=bold " active-to-inactive end
    hi User13  ctermfg=231  ctermbg=-1   cterm=bold " last active end
    hi User14  ctermfg=7    ctermbg=237  cterm=bold " inactive start
    hi User15  ctermfg=237  ctermbg=231  cterm=bold " before active end
    hi User16  ctermfg=237  ctermbg=-1   cterm=bold " last inactive end

--]]

local function fmt(info)
    local ntabs = vifm.tabs.getcount({ other = info.other })
    local ctab = vifm.tabs.getcurrent({ other = info.other })
    local format = ''
    for idx = 1, ntabs do
        local tab = vifm.tabs.get({ index = idx, other = info.other })
        local view = tab:getview()

        local name = tab:getname()
        if #name == 0 then
            name = vifm.fnamemodify(view.cwd, ':t') .. '/'
        end

        local cv = view.custom
        if cv ~= nil then
            local title = cv.title
            if cv.type == 'tree' then
                title = 'tree'
            end
            name = string.format('[%s] %s', title, name)
        end

        name = string.gsub(name, '%%', '%%%%')

        local islast = (idx == ntabs)
        local active = (idx == ctab)

        local startc = active and 11 or 14

        local endc
        if active then
            endc = islast and 13 or 12
        else
            endc = islast and 16 or 15
        end

        -- special case: inactive tab followed by another inactive tab
        local tail = ''
        if not active and not islast and idx ~= ctab - 1 then
            tail = ''
            endc = 14
        end

        local label = string.format('%%%d* %d: %s %%%d*%s',
                                    startc, idx, name, endc, tail)
        format = format .. label
    end
    return { format = format }
end

local added = vifm.addhandler {
    name = 'fmt',
    handler = fmt,
}
if not added then
    vifm.sb.error('Failed to register #%s#fmt', vifm.plugin.name)
end

return {}
