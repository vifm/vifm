--[[

Provides :Unpack command that unpacks tar-archive under the cursor taking care
of a situation when archive lacks a root-level directory.

Usage example:

    :Unpack
    :Unpack /tmp

--]]

-- TODO: a way to specify default path for unpacking
-- TODO: fix processing archives with spaces in their name
-- TODO: support .tgz and similar extensions in addition to current .tar.*

local M = {}

local function get_common_prefix(archive, format)
    local cmd
    if format == 'tar' then
        cmd = string.format('tar tf %q', archive)
    elseif format == 'zip' then
        cmd = string.format('zip --show-files %q', archive)
    elseif format == 'rar' then
        cmd = string.format('unrar vb %q', archive)
    else
        return nil, 'Unsupported format: '..format
    end

    local job = vifm.startjob { cmd = cmd }

    local prefix
    local prefix_len
    for line in job:stdout():lines() do
        local skip = false

        print('line:'..line)
        if format == 'zip' then
            if line:sub(1, 2) ~= '  ' then
                skip = true
            else
                line = line:sub(3)
            end
        end
        print('line:'..line)

        if not skip then
            if prefix == nil then
                prefix = line
                local top = prefix:match("(.-/)")
                if top ~= nil then
                    prefix = top
                end
                prefix_len = #prefix
            end

            if line:sub(1, prefix_len) ~= prefix then
                job:wait()
                return nil
            end
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

    local exitcode = job:exitcode()
    if exitcode ~= 0 then
        print('Listing failed with exit code: '..exitcode)
        return prefix, 'errors:'..job:errors()
    end

    return prefix
end

local function unpack(info)
    local view = vifm.currview()

    local current = vifm.expand('%c:p')
    if #current == 0 then
        vifm.sb.error('There is no current file')
        return
    end

    local ext = vifm.fnamemodify(current, ':t:e')
    if ext ~= 'zip' and ext ~= 'rar' then
        ext = vifm.fnamemodify(current, ':t:r:e')
        if ext ~= 'tar' then
            vifm.sb.error('Unsupported file format')
            return
        end
    end

    local prefix, err = get_common_prefix(current, ext)
    if err ~= nil then
        vifm.sb.error(err)
        return
    end

    local outdir = '.'
    if #info.argv ~= 0 then
        outdir = info.argv[1]
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

    local cmd
    if ext == 'tar' then
        cmd = string.format('tar -C %q -vxf %q', outdir, current)
    elseif ext == 'zip' then
        cmd = string.format('unzip -d %q %q', outdir, current)
    elseif ext == 'rar' then
        cmd = string.format('cd %q && unrar x %q', outdir, current)
    end
    local job = vifm.startjob { cmd = cmd }

    for line in job:stdout():lines() do
        vifm.sb.quick(line)
    end

    if job:exitcode() == 0 then
        if prefix ~= nil then
            outdir = outdir..'/'..prefix
        end
        view:cd(outdir)
    else
        local errors = job:errors()
        if #errors == 0 then
            vifm.errordialog('Unpacking failed',
                             'Error message is not available.')
        else
            vifm.errordialog('Unpacking failed', errors)
        end

        if prefix == nil then
            view:cd(outdir)
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
