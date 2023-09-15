--[[

Enables use of Ueberzug for viewing images:
 - starts Ueberzug on the first use
 - prints an error message to preview area if command isn't found
 - restarts Ueberzug if it exits (you also get an error message about it)

Ueberzug:
 - original (discontinued): https://github.com/seebye/ueberzug/
 - fork of python version:  https://github.com/ueber-devel/ueberzug
 - improved rewrite in C++: https://github.com/jstkdng/ueberzugpp

Usage example:
    " If you are using ueberzugpp, you can have animated gif previews with this:
    fileviewer <image/gif>
        \ #ueberzug#image_no_cache %px %py %pw %ph
        \ %pc
        \ #ueberzug#clear

    " Otherwise, use the video fileviewer for gifs
    " fileviewer <video/*>,<image/gif>
    fileviewer <video/*>
        \ #ueberzug#video %px %py %pw %ph
        \ %pc
        \ #ueberzug#clear

    fileviewer <image/*>
        \ #ueberzug#image %px %py %pw %ph
        " or \ #ueberzug#image_no_cache %px %py %pw %ph
        \ %pc
        \ #ueberzug#clear
    fileviewer <audio/*>
        \ #ueberzug#audio %px %py %pw %ph
        \ %pc
        \ #ueberzug#clear
    fileviewer <font/*>
        \ #ueberzug#djvu %px %py %pw %ph
        \ %pc
        \ #ueberzug#clear

    fileviewer *.pdf
        \ #ueberzug#pdf %px %py %pw %ph
        \ %pc
        \ #ueberzug#clear
    fileviewer *.epub
        \ #ueberzug#epub %px %py %pw %ph
        \ %pc
        \ #ueberzug#clear
    fileviewer *.djvu
        \ #ueberzug#djvu %px %py %pw %ph
        \ %pc
        \ #ueberzug#clear

    fileviewer *.cbz
        \ #ueberzug#cbz %px %py %pw %ph
        \ %pc
        \ #ueberzug#clear
    fileviewer *.cbt
        \ #ueberzug#cbt %px %py %pw %ph %c
        \ %pc
        \ #ueberzug#clear
    fileviewer *.cbr
        \ #ueberzug#cbr %px %py %pw %ph %c
        \ %pc
        \ #ueberzug#clear
    fileviewer *.cb7
        \ #ueberzug#cb7 %px %py %pw %ph %c
        \ %pc
        \ #ueberzug#clear

--]]

-- TODO: maybe don't register handlers if executable isn't present to allow
--       other viewers to kick in
-- TODO: generate JSON properly
-- TODO: stop Ueberzug after some period of inactivity (needs timer API?)
-- TODO: try not waiting for previewers and poking ueberzug inside "onexit"
--       handler of previewer's job

local M = {}
local layer_id = 'vifm-preview'
local pipe_storage
local cache_path_storage

local function get_pipe()
    if pipe_storage == nil then
        local cmd
        if vifm.executable('ueberzugpp') then
            cmd = 'ueberzugpp layer --no-cache'
        elseif vifm.executable('ueberzug') then
            cmd = 'ueberzug layer'
        else
            return nil, "ueberzug executable isn't found"
        end

        local uberzug = vifm.startjob {
            cmd = cmd,
            iomode = 'w',
            onexit = function()
                pipe_storage = nil
                vifm.errordialog('ueberzug plugin',
                                 'ueberzug has exited unexpectedly.\n'..
                                 'It will be restarted on the next use.')
            end
        }
        pipe_storage = uberzug:stdin()
    end
    return pipe_storage
end

local function get_cache_path()
    if cache_path_sorage == nil then
        local cache_path = os.getenv('XDG_CACHE_HOME')
        if not cache_path then
          cache_path = os.getenv('HOME')..'/.cache'
        end
        cache_path_storage = cache_path..'/vifm/'
        vifm.makepath(cache_path_storage)
    end
    return cache_path_storage
end

local function clear(info)
    local pipe, err = get_pipe()
    if pipe == nil then
        return { lines = { err } }
    end

    local format = '{"action":"remove", "identifier":"%s"}\n'
    local message = format:format(layer_id)
    pipe:write(message)
    pipe:flush()
    return { lines = {} }
end

local function view(info)
    local pipe, err = get_pipe()
    if pipe == nil then
        return { lines = { err } }
    end

    local format = '{"action":"add", "identifier":"%s",'
                 ..' "x":%d, "y":%d, "width":%d, "height":%d,'
                 ..' "path":"%s"}\n'
    local message = format:format(layer_id,
                                  info.x, info.y, info.width, info.height,
                                  info.path)
    pipe:write(message)
    pipe:flush()
    return { lines = {} }
end

local function cached_view_with(info, ext, thumbnailer)
    local function get_thumb_path_without_extension(path)
        local cache_path = get_cache_path()
        local job = vifm.startjob {
            cmd = 'stat --printf "%n%i%F%s%W%Y" -- "'..path..'" | sha512sum',
        }
        local sha = job:stdout():read('*all'):sub(1, -5)
        return cache_path..sha
    end

    local function file_exists(path)
       local f = io.open(path, "r")
       return f ~= nil and io.close(f)
    end

    local thumb_partial_path = get_thumb_path_without_extension(info.path)
    local thumb_full_path = thumb_partial_path..ext
    if not file_exists(thumb_full_path) then
        thumbnailer(thumb_full_path, thumb_partial_path, ext)
    end
    info.path = thumb_full_path
    return view(info)
end

local function cached_view(info, fmt)
    local thumbnailer = function(thumb_full_path)
        vifm.startjob { cmd = fmt:format(info.path, thumb_full_path) }:wait()
    end
    cached_view_with(info, '.jpg', thumbnailer)
end

local function cached_view_for_archives(info, mount_fmt)
    local thumbnailer = function(thumb_full_path)
        local mount_cmd = mount_fmt:format(info.path)
        local cmd = ([[
            tmpdir="$(mktemp --directory)"
            cleanup() { 
                tmpdir="$1"
                mountpoint -q -- "$tmpdir"
                is_mounted=$?
                [ "$is_mounted" = 0 ] && fusermount -u "$tmpdir"
                rm -rf "$tmpdir"
            }
            trap "cleanup $tmpdir" EXIT

            %s
            first_image="$(find "$tmpdir" -type f \( -iname "*.png" -o -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.webp" \) | sort | head -1)"
            convert -thumbnail 720 "$first_image" "%s"
        ]]):format(mount_cmd, thumb_full_path)
        vifm.startjob { cmd = cmd }:wait()
    end

    cached_view_with(info, '.jpg', thumbnailer)
end

local function image_no_cache(info)
    return view(info)
end
local function image(info)
    return cached_view(info, 'convert -thumbnail 720 "%s" "%s"')
end
local function video(info)
    return cached_view(info, 'ffmpegthumbnailer -i "%s" -o "%s" -s 720 -q 5')
end
local function audio(info)
    return cached_view(info, 'ffmpeg -hide_banner -i "%s" "%s" -y >/dev/null')
end
local function font(info)
    return cached_view(info, 'fontpreview -i "%s" -o "%s"')
end

local function epub(info)
    return cached_view(info, 'gnome-epub-thumbnailer "%s" "%s" -s 720')
end
local function djvu(info)
    return cached_view(info, 'ddjvu -format=tiff -quality=90 -page=1 "%s" "%s"')
end
local function pdf(info)
    local thumbnailer = function(_, thumb_partial_path)
        local fmt = 'pdftoppm -jpeg -f 1 -singlefile "%s" "%s"'
        local cmd = fmt:format(info.path, thumb_partial_path)
        vifm.startjob { cmd = cmd }:wait()
    end
    return cached_view_with(info, ".jpg", thumbnailer)
end

local function cbz(info)
    return cached_view_for_archives(info, 'fuse-zip -r "%s" "$tmpdir"')
end
local function cbt(info)
    return cached_view_for_archives(info, 'archivemount "%s" "$tmpdir" -o readonly')
end
local function cbr(info)
    return cached_view_for_archives(info, 'rar2fs "%s" "$tmpdir"')
end
local function cb7(info)
    return cached_view_for_archives(info, 'fuse3-p7zip "%s" "$tmpdir"')
end

local function handle(info)
    local added = vifm.addhandler(info)
    if not added then
        vifm.sb.error(string.format('Failed to register #%s#%s',
                                    vifm.plugin.name, info.name))
    end
end

handle { name = 'clear', handler = clear }

handle { name = 'image', handler = image }
handle { name = 'image_no_cache', handler = image_no_cache }
handle { name = 'video', handler = video }
handle { name = 'audio', handler = audio }
handle { name = 'font', handler = font }

handle { name = 'pdf', handler = pdf }
handle { name = 'epub', handler = epub }
handle { name = 'djvu', handler = djvu }

handle { name = 'cbz', handler = cbz }
handle { name = 'cbt', handler = cbt }
handle { name = 'cbr', handler = cbr }
handle { name = 'cb7', handler = cb7 }

return M
