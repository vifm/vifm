vifm.plugin.require('pack')
vifm.plugin.require('unpack')

-- this is used by code in other files
function unescape_name(name)
   return name:gsub('\\(.)', '%1')
end

local added = vifm.cmds.add {
   name = "Unpack",
   description = "unpack an archive into specified directory",
   handler = unpack,
   minargs = 0,
   maxargs = 2,
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

return {}

-- vim: set et ts=3 sts=3 sw=3:
