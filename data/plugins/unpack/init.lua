--[[

Provides :Unpack command that unpacks tar-archive under the cursor taking care
of a situation when archive lacks a root-level directory.

Usage example:

    :Unpack
    :Unpack /tmp

--]]

-- TODO: a way to specify default path for unpacking
-- TODO: support .tgz and similar extensions in addition to current .tar.*
-- TODO: support .zip and .rar as well

local M = {}

local function get_common_prefix(archive)
    local job = vifm.startjob {
        cmd = string.format('tar tf %q', archive)
    }

    local lines = job:stdout():lines()
    local prefix = lines()
    if prefix == nil then
        job:wait()
        return nil, 'Failed to list contents of '..archive..':\n'..job:errors()
    end

    local prefix_len = #prefix
    for line in lines do
        if line:sub(1, prefix_len) ~= prefix then
            prefix = nil
            break
        end
    end

    if prefix ~= nil then
        if prefix:sub(-1) ~= '/' then
            prefix = nil
        else
            prefix = prefix:sub(1, #prefix - 1)
            if prefix == '.' then
                prefix = nil
            end
        end
    end

    if job:exitcode() ~= 0 then
        return prefix, job:errors()
    end

    return prefix
end

local function unpack(info)
    local current = vifm.expand('%c:p')
    if #current == 0 then
        vifm.sb.error('There is no current file')
        return
    end

    local ext = vifm.fnamemodify(current, ':t:r:e')
    if ext ~= 'tar' then
        vifm.sb.error('Unsupported file format')
        return
    end

    local prefix, err = get_common_prefix(current)
    if err ~= nil then
        vifm.sb.error(err)
        return
    end

    local outdir = info.args
    if #outdir == 0 then
        outdir = '.'
    end
    if not vifm.exists(outdir) then
        vifm.sb.error("Output path doesn't exist")
        return
    end

    local name = prefix
    if name == nil then
        name = vifm.fnamemodify(current, ':t:r:r')
    end

    local dirpath = outdir..'/'..name
    if vifm.exists(dirpath) then
        vifm.sb.error(string.format('Output path already exists: %s', dirpath))
        return
    end

    if prefix == nil then
        outdir = outdir..'/'..name
        if not vifm.makepath(outdir) then
            vifm.sb.error(
                string.format('Failed to create output directory: %s', dirpath)
            )
            return
        end
    end

    local job = vifm.startjob {
        cmd = string.format('tar -C %q -vxf %q', outdir, current)
    }

    for line in job:stdout():lines() do
        vifm.sb.quick(line)
    end

    if job:exitcode() == 0 then
        if prefix ~= nil then
            outdir = outdir..'/'..prefix
        end
        vifm.cd(outdir)
    else
        local errors = job:errors()
        if #errors == 0 then
            vifm.errordialog('Unpacking failed',
                             'Error message is not available.')
        else
            vifm.errordialog('Unpacking failed', errors)
        end

        if prefix == nil then
            vifm.cd(outdir)
        end
    end
end

-- this does NOT overwrite pre-existing user command
local added = vifm.cmds.add {
    name = "Unpack",
    description = "unpack an archive into a single directory",
    handler = unpack,
    maxargs = 1,
}
if not added then
    vifm.sb.error("Failed to register :Unpack")
end

return M
