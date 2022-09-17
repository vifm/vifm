--[[

Adds the :Run command that uses Zsh for shell commands' completion.

--]]

local escaped_plugin_path = vifm.escape(vifm.plugin.path)

local function zsh_complete(info)
    local env = ''
    if vifm.expand('$ZSH_CAPTURE_COMPDUMP') == '$ZSH_CAPTURE_COMPDUMP' then
        env = 'ZSH_CAPTURE_COMPDUMP=' .. escaped_plugin_path .. '/zcompdump '
    end

    local job = vifm.startjob {
        cmd = string.format('%s %s %s',
                            env,
                            escaped_plugin_path .. '/capture.zsh',
                            vifm.escape(info.args))
    }

    local matches = {}
    for line in job:stdout():lines() do
        line = line:gsub('\r$', '')
        local match, desc
        local i, j = line:find(' -- ', 1, true)
        if i == nil then
            match = line
        else
            match, desc = line:sub(1, i - 1), line:sub(j + 1)
        end
        matches[#matches + 1] = {match = match, description = desc}
    end

    local exitcode = job:exitcode()
    if exitcode ~= 0 then
        vifm.errordialog('Error', job:errors())
        return false
    end

    return {
        offset = info.args:len() - info.arg:len(),
        matches = matches,
    }
end

local function run(info)
    vifm.run {
        cmd = vifm.expand(info.args),
    }
end

local added = vifm.cmds.add {
    name = 'Run',
    description = 'complete shell commands using Zsh',
    handler = run,
    complete = zsh_complete,
    minargs = 1,
    maxargs = -1,
}

if not added then
    vifm.sb.error('Failed to register :Run')
end

return {}
