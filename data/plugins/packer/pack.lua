function pack(info) -- <<<
   local files = vifm.expand('%"f')
   if #files == 0 or
         files == '.' or files == './' or
         files == '..' or files == '../' then
      vifm.sb.error('Error: no files are selected')
      return
   end

   local outfile
   local ext = 'tar.gz' -- default extension
   if #info.argv == 1 then
      outfile = unescape_name(vifm.expand(info.argv[1]))
      ext = vifm.fnamemodify(outfile, ':e')
      if vifm.fnamemodify(outfile, ':r:e') == 'tar' then
         ext = 'tar.'..ext
      end
   else
      local singlefile = (vifm.expand('%"c') == files)
      local basename = singlefile and vifm.expand('%c:r:r') or vifm.expand('%d:t')
      outfile = string.format("%s.%s", unescape_name(basename), ext)
   end
   if vifm.exists(outfile) then
      vifm.errordialog(":Pack", string.format("File already exists: %s", outfile))
      return
   end

   local eoutfile = vifm.escape(outfile)

   local cmd
   if ext == 'tar.gz' or ext == 'tgz' then
      cmd = string.format("tar --force-local -cvzf %s %s", eoutfile, files)
   elseif ext == 'tar.bz2' or ext == 'tbz2' then
      cmd = string.format("tar --force-local -cvjf %s %s", eoutfile, files)
   elseif ext == 'tar.xz' or ext == 'txz' then
      cmd = string.format("tar --force-local -cvJf %s %s", eoutfile, files)
   elseif ext == 'tar.zst' or ext == 'tzst' then
      cmd = string.format("tar -I 'zstd -19' --force-local -cvf %s %s",
                          eoutfile, files)
   elseif ext == "tar" then
      cmd = string.format("tar --force-local -cvf %s %s", eoutfile, files)
   elseif ext == "7z" or
         ext == "lz4" or
         ext == "zip" then
      cmd = string.format("7z a %s %s", eoutfile, files)
   else -- TODO: rar, gzip, bzip2, xz, zst etc
      vifm.sb.error(string.format("Unknown format: %s", ext))
      return
   end

   local job = vifm.startjob {
      cmd = cmd,
      description = "Packing: "..outfile,
      visible = true,
      iomode = '', -- ignore output to not block
      onexit = function(job)
          if job:exitcode() ~= 0 then
             local errors = job:errors()
             if #errors == 0 then
                vifm.errordialog('Packing failed',
                                 'Error message is not available.')
             else
                vifm.errordialog('Packing failed', errors)
             end
          end
      end
   }
end -- >>>

-- vim: set et ts=3 sts=3 sw=3:
