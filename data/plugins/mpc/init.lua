--[[

Provides commands and a handler to call mpc.

    " add files after the current one
    :mpcinsert
    " play next song in playlist
    :mpcnext
    " start playing next song
    :mpcplaynext

    filetype {*.wav,*.mp3,*.flac,*.m4a,*.wma,*.ape,*.ac3,*.og[agx],*.spx,
             \*.opus,*.aif}
        \ {Insert after current in mpd playlist and play}
        \ #mpc#run insert playnext

--]]

-- TODO: check for presence of `mpc`
-- TODO: maybe don't parse MPD configuration until first use

local M = {}

local musicdir

function mpcinsert()
    local cmd = string.format('realpath -s --relative-to=%s %s | mpc insert',
                              vifm.escape(musicdir),
                              vifm.expand("%f"))
    local job = vifm.startjob { cmd = cmd }
    job:wait()
end

function mpcnext()
    local job = vifm.startjob { cmd = 'mpc next' }
    job:wait()
end

function mpcplaynext()
    local job = vifm.startjob { cmd = 'mpc | wc -l' }
    local n = tonumber(job:stdout():lines()())
    job:wait()

    if n == 1 then
        job = vifm.startjob { cmd = 'mpc play' }
        job:wait()
    else
        mpcnext()
    end
end

function addcmd(info)
    -- this does NOT overwrite pre-existing user command
    local added = vifm.cmds.add(info)
    if not added then
        vifm.sb.error(string.format("Failed to register :%s", info.name))
    end
end

local function run(info)
    local actions = string.gmatch(info.command, "(%S+)")
    for action in actions do
        if action == 'insert' then
            mpcinsert()
        elseif action == 'next' then
            mpcnext()
        elseif action == 'playnext' then
            mpcplaynext()
        end
    end
end

for cfg in io.lines(vifm.expand('$HOME/.mpd/mpd.conf')) do
    option, value = string.match(cfg, '%s*(%S+)%s+"([^"]+)"')
    vifm.sb.error(tostring(option) .. ' ' .. tostring(value))
    if option == 'music_directory' then
        musicdir = value
        break
    end
end

if musicdir == nil then
    error('Failed to determine MPD database directory')
end

if musicdir:sub(1, 1) == '~' then
    musicdir = vifm.expand('$HOME') .. musicdir:sub(2)
end
vifm.sb.error(tostring(musicdir))

addcmd {
    name = "mpcinsert",
    description = "add files after the current one",
    handler = mpcinsert,
}
addcmd {
    name = "mpcnext",
    description = "play next song in playlist",
    handler = mpcnext,
}
addcmd {
    name = "mpcplaynext",
    description = "start playing next song",
    handler = mpcplaynext,
}

local added = vifm.addhandler {
    name = "run",
    handler = run,
}
if not added then
    vifm.sb.error(string.format("Failed to register #%s#run", vifm.plugin.name))
end

return M
