--[[

Allows using Vifm as an IDE-like file-manager/file-tree for Helix

Unimplemented actions:
 * open-help (:help when 'vimhelp' is set):
 * edit-list ("v" key in navigation menus):

--]]

local M = {}

M.handlers = {}

M.handlers["open-help"] = function(info)
  vifm.errordialog("Unimplemented",
    "Can't view Vim-style help via Helix.\n" ..
    "Do `:set novimhelp` to make argumentless `:help` work.")
  return false
end

local function prepare_path(path)
  -- turn to full path and escape it for pasting into a shell command
  return vifm.escape(vifm.fnamemodify(path, ':p'))
end

M.handlers["edit-one"] = function(info)
  local posarg = ""
  if info.line ~= nil and info.column == nil then
    posarg = ':' .. info.line
  elseif info.line ~= nil and info.column ~= nil then
    posarg = string.format(":%d:%d", info.line, info.column)
  end

  local tmux_session_id = "$(tmux display -p '#S')"
  local job = vifm.startjob {
    cmd = string.format("tmux send-keys -t %s:editor.1 ':open %s%s' Enter",
      tmux_session_id, prepare_path(info.path), posarg),
    usetermmux = not info.mustwait,
  }

  return job:exitcode() == 0
end

function map(table, functor)
  local result = {}
  for key, value in pairs(table) do
    result[key] = functor(value)
  end
  return result
end

M.handlers["edit-many"] = function(info)
  local tmux_session_id = "$(tmux display -p '#S')"
  local job = vifm.startjob {
    cmd = string.format("tmux send-keys -t %s:editor.1 ':open %s' Enter",
      tmux_session_id, table.concat(map(info.paths, prepare_path), ' '))
  }

  return job:exitcode() == 0
end

M.handlers["edit-list"] = function(info)
  vifm.errordialog("Unimplemented", "Can't view quickfix via Helix.")
  return false
end

return M
