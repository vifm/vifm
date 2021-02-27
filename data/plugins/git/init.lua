--[[

Provides :Gclone command that clones repository by its URL determining directory
name from URL and enters cloned directory on success.

Usage example:

    Clone into directory called "vifm".
    :Gclone https://github.com/vifm/vifm.git

    Clone into directory called "vifm-dev".
    :Gclone https://github.com/vifm/vifm.git vifm-dev

--]]

local M = {}

local function clone(info)
    local url = info.argv[1]

    local name
    if #info.argv > 1 then
        name = info.argv[2]
    else
        name = vifm.fnamemodify(url, ":t:s/\\.git$//", "")
    end

    vifm.sb.quick(string.format('Cloning %q to %q...', url, name))

    local job = vifm.startjob {
        cmd = string.format('git clone %q %q', url, name)
    }
    for line in job:stdout():lines() do
        vifm.sb.quick(line)
    end

    if job:exitcode() == 0 then
        vifm.currview():cd(name)
    else
        local errors = job:errors()
        if #errors == 0 then
            vifm.errordialog('Gclone failed to clone',
                             'Error message is not available.')
        else
            vifm.errordialog('Gclone failed to clone', errors)
        end
    end
end

-- this does NOT overwrite pre-existing user command
local added = vifm.cmds.add {
    name = "Gclone",
    description = "clone a repository and enter it",
    handler = clone,
    minargs = 1,
    maxargs = 2,
}
if not added then
    vifm.sb.error("Failed to register :Gclone")
end

return M
