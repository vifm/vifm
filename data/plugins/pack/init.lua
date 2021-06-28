--[[

Provides :Pack command that archives selection or current file.  Name of a
single file is used to name the archive, for multiple files name of current
directory is used.

Usage example:

    :Pack

--]]

-- TODO: allow specifying custom name
-- TODO: check presence of the applications
-- TODO: need a way to report failures (callbacks are necessary after all?)
-- TODO: displaying progress would be nice

local M = {}

local function pack(info)
    local files = vifm.expand('%f')
    -- TODO: get rid of this hack
    local singlefile = (vifm.expand('%c') == files)
    local basename = singlefile and vifm.expand('%c') or vifm.expand('%d:t')

    local ext = info.argv[1]
    local outfile = basename.."."..ext

    if vifm.exists(outfile) then
        vifm.errordialog(":Pack",
                         string.format("File already exists: %s", outfile))
        return
    end

    local cmd
    if ext == 'tar.bz2' or ext == 'tbz2' then
        cmd = string.format("tar cvjf %s %s", outfile, files)
    elseif ext == "tar.gz" or ext == "tgz" then
        cmd = string.format("tar cvzf %s %s", outfile, files)
    elseif ext == "bz2" then
        cmd = string.format("bzip2 %s %s", outfile, files)
    elseif ext == "gz" then
        cmd = string.format("gzip %s %s", outfile, files)
    elseif ext == "tar" then
        cmd = string.format("tar cvf %s %s", outfile, files)
    elseif ext == "zip" then
        cmd = string.format("zip -r %s %s", outfile, files)
    elseif ext == "Z" then
        cmd = string.format("compress %s %s", outfile, files)
    elseif ext == "7z" then
        cmd = string.format("7z a %s %s", outfile, files)
    else
        vifm.sb.error(string.format("Unknown format: %s", ext))
        return
    end

    local job = vifm.startjob {
        cmd = cmd,
        description = string.format("Archiving %s", outfile),
        visible = true,
        iomode = '' -- ignore output to not block
    }
end

-- this does NOT overwrite pre-existing user command
local added = vifm.cmds.add {
    name = "Pack",
    description = "archive selection or current file",
    handler = pack,
    minargs = 1,
}
if not added then
    vifm.sb.error("Failed to register :Pack")
end

return M
