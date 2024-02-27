--[[

-------------------------------------------------------------------
Plugin to pack and unpack archives on the fly

Provides :Pack command that archives selection or current file.
name of a single file is used to name the archive, for multiple
files name of current directory is used. :Pack optionally takes
an argument. if argument is given, it will be used as archive's
name instead of the previous options. eg. :Pack foobar.zip

Provides :Unpack command that unpacks common archives like tarballs,
zip, rar, 7z nicely in a subdir. Supports unpacking multiple files.
-------------------------------------------------------------------

Requirements:
   `7z`  - p7zip for most of decompression, including rar to
         extract rar files, p7zip must be build with rar support.
         also for packing into zip, 7z, lz4 etc type compressions.
         Why 7z? well it supports wide range of archives, it is
         fast and efficient at decompression.

   `tar` - for (un)packing compressed tarballs. currently supports
         gzip, xz, bzip2 and zstd compressed tarballs.

   `awk` is required as well. TODO: test using non-GNU awk

Features:
   - unpack zip, rar, 7z, tar.gz, tar.bz2 etc
   - unpack multiple archives on the fly
   - archives are unpacked nicely in a subdirectory
   - doesn't overwrite existing files
   - supports unpacking archives with special characters

Limitations:
   - extracting large tarballs is slow due to `tar tf` being slow
   - packing doesn't work well with filenames having special characters

Commands:
   :Unpack <dir>
   :Unpack /tmp
            extracts archive into a subdir or <dir> if given

   :Pack <archive-name>
   :Pack foobar.tar.gz
            archives the selected files. optionally takes archive name

Features under consideration:
   - password support
   - ask to overwrite or merge when (un)packing

Status:
   - working under linux environment. tested on gentoo
   - not tested on windows. please report if it doesn't work

--]]

local M = {}

local function escape_name(name)
   return name:gsub('([$*#!&?|\\(){}<>["\'%s])', '\\%1'):gsub('(])', '\\%1')
end
local function unescape_name(name)
   return name:gsub('\\([$*#!&?|\\(){}<>["\'%s])', '%1'):gsub('\\(])', '%1')
end

local function get_common_unpack_prefix(archive, format) -- <<<
   -- cmd should output the list of contents in the archive
   -- directory must contain '/' at end. adjust cmd accordingly
   local cmd
   if format == 'tar' then
      -- this is slow in comparison to 7z. noticably slow
      -- when large archives or archives with lots of files
      cmd = string.format("tar tf %s", archive)
   elseif format == 'zip' or format == 'rar' or format == '7z' then
      cmd = string.format("7z -ba l %s | awk '{($3 ~ /^D/) ? $0=$0\"/\" : $0; a=match($0, $6); print substr($0,a) }'", archive)
   else
      return nil, 'unsupported format: '..format
   end

   local job = vifm.startjob { cmd = cmd }
   local prefix
   local prefix_len
   for line in job:stdout():lines() do
      vifm.sb.quick("Checking: "..line)
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
      return prefix, 'errors: '..job:errors()
   end

   return prefix
end -- >>>

local function unpack_archive(archive, target) -- <<<
   local fpath = archive
   local fname = vifm.fnamemodify(fpath, ':t')
   local fdir = vifm.fnamemodify(fpath, ':h')

   local ext
   local cmp
   ext = vifm.fnamemodify(fname, ':e')
   -- if tarball; ext -> 'tar'
   if ext == 'tgz' or
         ext == 'txz' or
         ext == 'tbz2' or
         ext == 'tzst' or
         vifm.fnamemodify(fname, ':r:e') == 'tar' then
      cmp = ext
      ext = 'tar'
   end

   local outdir = fdir
   if target ~= nil then
      -- TODO: vifm doesn't expand '~', find way to fix
      outdir = vifm.fnamemodify(vifm.expand(target), ':p')
   end
   if not vifm.exists(unescape_name(outdir)) then
      vifm.errordialog(":Unpack", "Error: output directory does not exists")
      return
   end

   local prefix, err = get_common_unpack_prefix(fpath, ext)
   if err ~= nil then
      vifm.sb.error(err)
      return
   end

   if prefix == nil then
      if ext == 'tar' then
         outdir = string.format("%s/%s", outdir, vifm.fnamemodify(fname, ':r:r'))
      else
         outdir = string.format("%s/%s", outdir, vifm.fnamemodify(fname, ':r'))
      end
      if vifm.exists(unescape_name(outdir)) then
         vifm.errordialog(":Unpack", string.format("Output directory already exist %%%%%s%%%%", outdir))
         return
      end
      if not vifm.makepath(unescape_name(outdir)) then
         vifm.errordialog(":Unpack", string.format('Failed to create output directory: %%%%%s%%%%', outdir))
         return
      end
   else
      prefix = escape_name(prefix)
      if vifm.exists(unescape_name(string.format("%s/%s", outdir, prefix))) then
         vifm.errordialog(":Unpack", string.format("Prefix directory already exist %%%%%s/%s%%%%", outdir, prefix))
         return
      end
   end

   local cmd
   if ext == 'tar' then
      if cmp == "tgz" or cmp == "gz" then
         cmd = string.format('tar -C %s -vxzf %s', outdir, fpath)
      elseif cmp == "tbz2" or cmp == "bz2" then
         cmd = string.format('tar -C %s -vxjf %s', outdir, fpath)
      elseif cmp == "txz" or cmp == "xz" then
         cmd = string.format('tar -C %s -vxJf %s', outdir, fpath)
      elseif cmp == "tzst" or cmp == "zst" then
         cmd = string.format("tar -C %s -I 'zstd -d' -vxf %s", outdir, fpath)
      else
         vifm.sb.error("Error: unknown compression format"..cmp)
         return
      end
   elseif ext == 'zip' or ext == 'rar' or ext == '7z' or ext == 'lz4' then
      cmd = string.format("cd %s && 7z -bd x %s", outdir, fpath)
   end

   local job = vifm.startjob { cmd = cmd }
   for line in job:stdout():lines() do
      vifm.sb.quick("Extracting: "..line)
   end

   if job:exitcode() ~= 0 then
      local errors = job:errors()
      if #errors == 0 then
         vifm.errordialog('Unpacking failed', 'Error message is not available.')
      else
         vifm.errordialog('Unpacking failed', errors)
      end
   end
end -- >>>

local function add_to_selection(selection, entry)
   if entry.type == 'exe' or entry.type == 'reg' or entry.type == 'link' then
      local path = string.format('%s/%s', entry.location, entry.name)
      table.insert(selection, path)
   end
end

local function get_selected_paths(view)
   local selection = { }
   local has_selection = false

   for i = 1, view.entrycount do
      local entry = view:entry(i)
      if entry.selected then
         has_selection = true
         add_to_selection(selection, entry)
      end
   end

   if not has_selection then
      add_to_selection(selection, view:entry(view.currententry))
   end

   return selection
end

local function unpack(info) -- <<<
   local archives = get_selected_paths(vifm.currview())
   if #archives == 0 then
      vifm.sb.error('Error: no file object under cursor')
      return
   end

   local target
   if #info.argv == 1 then
      target = vifm.fnamemodify(vifm.expand(info.argv[1]), ':p')
   end
   for _, archive in ipairs(archives) do
      unpack_archive(archive, target)
   end
end -- >>>

local function pack(info) -- <<<
   local files = vifm.expand('%f')
   if #files == 0 or
         files == '.' or files == './' or
         files == '..' or files == '../' then
      vifm.sb.error('Error: no files are selected')
      return
   end

   local outfile
   local ext = 'tar.gz' -- default extension
   if #info.argv == 1 then
      outfile = info.argv[1]
      ext = vifm.fnamemodify(outfile, ':e')
      if vifm.fnamemodify(outfile, ':r:e') == 'tar' then
         ext = 'tar.'..ext
      end
      outfile = escape_name(outfile)
   else
      local singlefile = (vifm.expand('%c') == files)
      local basename = singlefile and vifm.expand('%c:r:r') or vifm.expand('%d:t')
      outfile = string.format("%s.%s", basename, ext)
   end
   if vifm.exists(unescape_name(outfile)) then
      vifm.errordialog(":Pack", string.format("File already exists: %s", outfile))
      return
   end

   local cmd
   if ext == 'tar.gz' or ext == 'tgz' then
      cmd = string.format("tar -cvzf %s %s", outfile, files)
   elseif ext == 'tar.bz2' or ext == 'tbz2' then
      cmd = string.format("tar -cvjf %s %s", outfile, files)
   elseif ext == 'tar.xz' or ext == 'txz' then
      cmd = string.format("tar -cvJf %s %s", outfile, files)
   elseif ext == 'tar.zst' or ext == 'tzst' then
      cmd = string.format("tar -I 'zstd -19' -cvf %s %s", outfile, files)
   elseif ext == "tar" then
      cmd = string.format("tar -cvf %s %s", outfile, files)
   elseif ext == "7z" or
         ext == "lz4" or
         ext == "zip" then
      cmd = string.format("7z a %s %s", outfile, files)
   else -- TODO: rar, gzip, bzip2, xz, zst etc
      vifm.sb.error(string.format("Unknown format: %s", ext))
      return
   end

   local job = vifm.startjob {
      cmd = cmd,
      description = "Packing archive: "..unescape_name(outfile), -- doesn't show up
      visible = true,
      iomode = '', -- ignore output to not block
   }

   if job:exitcode() ~= 0 then
      local errors = job:errors()
      if #errors == 0 then
         vifm.errordialog('Packing failed', 'Error message is not available.')
      else
         vifm.errordialog('Packing failed', errors)
      end
   end
end -- >>>

local added = vifm.cmds.add {
   name = "Unpack",
   description = "unpack an archive into specified directory",
   handler = unpack,
   minargs = 0,
   maxargs = 1,
}
if not added then
   vifm.sb.error("Failed to register :Unpack")
end
added = vifm.cmds.add {
   name = "Pack",
   description = "pack selected files and directories into an archive",
   handler = pack,
   minargs = 0,
   maxargs = 1,
}
if not added then
   vifm.sb.error("Failed to register :Pack")
end

return M

-- vim: set et sts=3 sw=3:
