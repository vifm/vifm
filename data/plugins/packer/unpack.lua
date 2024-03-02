local function get_common_unpack_prefix(archive, format) -- <<<
   -- cmd should output the list of contents in the archive
   -- directory must contain '/' at end. adjust cmd accordingly
   local cmd
   if format == 'tar' then
      -- this is slow in comparison to 7z. noticably slow
      -- when large archives or archives with lots of files
      cmd = string.format("tar --force-local -tf %s", vifm.escape(archive))
   elseif format == 'zip' or format == 'rar' or format == '7z' then
      cmd = string.format("7z -ba l %s | awk '{($3 ~ /^D/) ? $0=$0\"/\" : $0; a=match($0, $6); print substr($0,a) }'",
                          vifm.escape(archive))
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

   local outdir = target or fdir
   if not vifm.exists(outdir) then
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
      if vifm.exists(outdir) then
         local msg = string.format("Output directory already exists:\n \n\"%s\"", outdir)
         vifm.errordialog(":Unpack", msg)
         return
      end
      if not vifm.makepath(outdir) then
         local msg = string.format('Failed to create output directory:\n \n\"%s\"', outdir)
         vifm.errordialog(":Unpack", msg)
         return
      end
   else
      if vifm.exists(string.format("%s/%s", outdir, prefix)) then
         local msg = string.format("Prefix directory already exists:\n \n\"%s/%s\"", outdir, prefix)
         vifm.errordialog(":Unpack", msg)
         return
      end
   end

   local eoutdir = vifm.escape(outdir)
   local efpath = vifm.escape(fpath)

   local cmd
   if ext == 'tar' then
      if cmp == "tgz" or cmp == "gz" then
         cmd = string.format('tar --force-local -C %s -vxzf %s', eoutdir, efpath)
      elseif cmp == "tbz2" or cmp == "bz2" then
         cmd = string.format('tar --force-local -C %s -vxjf %s', eoutdir, efpath)
      elseif cmp == "txz" or cmp == "xz" then
         cmd = string.format('tar --force-local -C %s -vxJf %s', eoutdir, efpath)
      elseif cmp == "tzst" or cmp == "zst" then
         cmd = string.format("tar --force-local -C %s -I 'zstd -d' -vxf %s", eoutdir, efpath)
      else
         vifm.sb.error("Error: unknown compression format"..cmp)
         return
      end
   elseif ext == 'zip' or ext == 'rar' or ext == '7z' or ext == 'lz4' then
      cmd = string.format("cd %s && 7z -bd x %s", eoutdir, efpath)
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

function unpack(info) -- <<<
   local archives = get_selected_paths(vifm.currview())
   if #archives == 0 then
      vifm.sb.error('Error: no file object under cursor')
      return
   end

   local target
   if #info.argv == 1 then
      -- TODO: vifm.expand() doesn't expand '~', find way to fix
      target = vifm.fnamemodify(unescape_name(vifm.expand(info.argv[1])), ':p')
   end
   for _, archive in ipairs(archives) do
      unpack_archive(archive, target)
   end
end -- >>>

-- vim: set et ts=3 sts=3 sw=3:
