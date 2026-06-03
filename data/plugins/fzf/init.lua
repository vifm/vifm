-- fzf-backed command palette, find and grep integration.

local M = {}

local KIND_COLUMN_WIDTH = 7
local CMDNAME_COLUMN_MIN_WIDTH = 10
local KEY_COLUMN_MIN_WIDTH = 10
local LIVE_GREP_MIN_LEN = 3
local MOUNT_HEADER_LIMIT = 5

local function fail(msg)
    vifm.sb.error(msg)
    return nil
end

local function executable(name)
    if not vifm.executable(name) then
        return fail(name .. " executable not found")
    end
    return true
end

local function quote(value)
    return vifm.escape(value or "")
end

local function split_lines(text)
    local lines = {}
    if text == nil or text == "" then
        return lines
    end

    text = text:gsub("\r\n", "\n")
    if text:sub(-1) ~= "\n" then
        text = text .. "\n"
    end

    for line in text:gmatch("(.-)\n") do
        lines[#lines + 1] = line
    end
    return lines
end

local function join_lines(lines)
    if #lines == 0 then
        return ""
    end
    return table.concat(lines, "\n") .. "\n"
end

local function write_file(path, contents)
    local file = io.open(path, "wb")
    if file == nil then
        return false
    end

    file:write(contents)
    file:close()
    return true
end

local function run_capture(cmd)
    local job = vifm.startjob { cmd = cmd }
    local out = job:stdout():read("*a") or ""
    job:wait()
    return out, job:exitcode()
end

local function run_term_capture(cmd)
    return vifm.run { cmd = cmd, capture = true }
end

local function remove_files(paths)
    for _, path in ipairs(paths) do
        if path ~= nil then
            os.remove(path)
        end
    end
end

local function make_temp(contents)
    local path = os.tmpname()
    if not write_file(path, contents or "") then
        return nil
    end
    return path
end

local function run_fzf(input_path, query, prompt, extra)
    local cmd = table.concat({
        "fzf",
        "--query=" .. quote(query or ""),
        "--prompt=" .. quote(prompt),
        "--layout=reverse",
        "--height=100%",
        extra or "",
        "< " .. quote(input_path),
    }, " ")
    return run_term_capture(cmd)
end

local function add_header(entries, title)
    entries[#entries + 1] = {
        display = "-- " .. title .. " --",
        kind = "header",
    }
end

local function command_width(commands)
    local width = CMDNAME_COLUMN_MIN_WIDTH
    for _, cmd in ipairs(commands) do
        if #cmd.name > width then
            width = #cmd.name
        end
    end
    return width
end

local function add_commands(entries, commands, kind, width)
    local fmt = string.format("%%-%ds :%%-%ds %%s",
        KIND_COLUMN_WIDTH, width)
    for _, cmd in ipairs(commands) do
        if cmd.kind == kind then
            local label = kind == "builtin" and "builtin" or "usercmd"
            entries[#entries + 1] = {
                display = string.format(fmt, label, cmd.name, cmd.description or ""),
                kind = "command",
                action = cmd.name,
                needs_args = (cmd.minargs or 0) > 0,
            }
        end
    end
end

local function add_keys(entries)
    local fmt = string.format("%%-%ds %%-%ds %%s",
        KIND_COLUMN_WIDTH, KEY_COLUMN_MIN_WIDTH)
    for _, key in ipairs(vifm.keys.list { mode = "normal" }) do
        if key.shortcut ~= "" and key.action == "" and key.description ~= "" then
            entries[#entries + 1] = {
                display = string.format(fmt, "key", key.shortcut, key.description),
                kind = "key",
                action = key.shortcut,
            }
        end
    end
end

local function find_entry(entries, display)
    for _, entry in ipairs(entries) do
        if entry.display == display then
            return entry
        end
    end
    return nil
end

local function escape_brackets(text)
    return (text:gsub("<", "<lt>"))
end

local function execute_palette_entry(entry)
    if entry == nil or entry.kind == "header" then
        return
    end

    if entry.kind == "key" then
        vifm.keys.send(entry.action)
        return
    end

    local action = escape_brackets(entry.action)
    if entry.needs_args then
        vifm.keys.send(":" .. action .. " ")
    else
        vifm.keys.send(":" .. action .. "<cr>")
    end
end

function M.command(info)
    if not executable("fzf") then
        return
    end

    local commands = vifm.cmds.list()
    local width = command_width(commands)
    local entries = {}

    add_header(entries, "User-defined commands")
    add_commands(entries, commands, "user", width)
    add_header(entries, "Builtin commands")
    add_commands(entries, commands, "builtin", width)
    add_header(entries, "Normal mode keys")
    add_keys(entries)

    local input = {}
    for _, entry in ipairs(entries) do
        input[#input + 1] = entry.display
    end

    local input_path = make_temp(join_lines(input))
    if input_path == nil then
        return fail("Failed to prepare fzf input")
    end

    local selected = run_fzf(input_path, info.args, "Command: ")
    remove_files { input_path }

    execute_palette_entry(find_entry(entries, selected))
end

local function make_find_cmd(root)
    -- stderr is redirected to /dev/null so that error messages don't get
    -- printed on top of the fzf UI (this would otherwise happen e.g. when rg
    -- stumbles on pseudo-files in /proc/*/net/* and prints
    -- "Invalid argument (os error 22)" for each one, making fzf unusable).
    if vifm.executable("rg") then
        -- --one-file-system avoids descending into mounted directories,
        -- --no-ignore and --hidden make rg behave closer to find,
        -- --no-messages suppresses non-fatal warnings from rg itself.
        return "rg --files -0 --hidden --no-ignore --no-messages " ..
            "--one-file-system -- " ..
            quote(root) .. " 2>/dev/null"
    end

    -- -xdev is the find equivalent of rg's --one-file-system.
    return "find " .. quote(root) .. " -xdev -type f -print0 2>/dev/null"
end

local function normalize_dir(path)
    while #path > 1 and path:sub(-1) == "/" do
        path = path:sub(1, -2)
    end
    return path
end

local function list_skipped_mounts(root)
    if not vifm.executable("awk") or not vifm.exists("/proc/mounts") then
        return {}
    end

    root = normalize_dir(root)
    local cmd = "awk -v root=" .. quote(root) .. " '" ..
        "{ mp=$2; gsub(/\\\\040/, \" \", mp); " ..
        "if (mp != root && (root == \"/\" || index(mp, root \"/\") == 1)) " ..
        "print mp }' /proc/mounts 2>/dev/null"
    local out = run_capture(cmd)
    local mounts = {}
    local seen = {}
    for _, mount in ipairs(split_lines(out)) do
        if mount ~= "" and not seen[mount] then
            seen[mount] = true
            mounts[#mounts + 1] = mount
        end
    end
    table.sort(mounts)
    return mounts
end

local function make_find_header(root)
    local mounts = list_skipped_mounts(root)
    if #mounts == 0 then
        return ""
    end

    local shown = {}
    local limit = math.min(#mounts, MOUNT_HEADER_LIMIT)
    for i = 1, limit do
        shown[#shown + 1] = mounts[i]
    end

    local extra = ""
    if #mounts > limit then
        extra = string.format(" (+%d more)", #mounts - limit)
    end

    return "--header=" .. quote("Skipped mount points: " ..
        table.concat(shown, ", ") .. extra)
end

local function stash_find_results(view, root, query, selected)
    vifm.sb.info("FzfFindStashed: saving matches for :copen...")

    local cmd = make_find_cmd(root) .. " | " ..
        "fzf --read0 --filter=" .. quote(query)
    local out = run_capture(cmd)
    local matches = split_lines(out)
    if #matches == 0 then
        return
    end

    local specs = {}
    local pos = nil
    for i, path in ipairs(matches) do
        specs[i] = path
        if path == selected then
            pos = i
        end
    end

    vifm.menus.loadcustom {
        title = "FindFZF " .. query .. " @ " .. root,
        items = matches,
        specs = specs,
        withnavigation = true,
        view = view,
        stash = true,
        pos = pos,
    }
end

local function find(info, stash)
    if not executable("fzf") then
        return
    end

    local view = vifm.currview()
    local root = view.cwd
    local query = info.args or ""
    local cmd = table.concat({
        make_find_cmd(root),
        "|",
        "fzf",
        "--read0",
        "--print-query",
        "--query=" .. quote(query),
        "--prompt=" .. quote("File: "),
        "--layout=reverse",
        "--height=100%",
        make_find_header(root),
    }, " ")

    local lines = split_lines(run_term_capture(cmd))
    if #lines == 0 then
        return
    end

    local next_query = lines[1] or query
    local picked = lines[2]
    if stash then
        stash_find_results(view, root, next_query, picked)
    end

    if picked ~= nil and not view:gotopath(picked) then
        fail("Path doesn't exist: " .. picked)
    end
end

function M.find(info)
    find(info, false)
end

function M.find_stashed(info)
    find(info, true)
end

local function make_grep_cmd(root, pattern)
    if vifm.executable("rg") then
        return "rg --line-number --column --with-filename " ..
            "--color=never --smart-case -- " .. quote(pattern) .. " " ..
            quote(root)
    end

    if vifm.executable("grep") then
        return "grep -RInH -I -e " .. quote(pattern) .. " " .. quote(root)
    end

    return nil
end

local function extract_grep_match(line)
    local start = 1
    while true do
        local from, to, number = line:find(":(%d+):", start)
        if from == nil then
            return nil
        end

        local path = line:sub(1, from - 1)
        if vifm.exists(path) then
            return path, tonumber(number)
        end
        start = to + 1
    end
end

local function prepare_grep_files(root, pattern)
    local grep_cmd = make_grep_cmd(root, pattern)
    if grep_cmd == nil then
        return fail("Neither rg nor grep executable found")
    end

    local out = run_capture(grep_cmd)
    local result_lines = {}
    local meta_lines = {}
    local id = 1
    for _, line in ipairs(split_lines(out)) do
        local path, line_number = extract_grep_match(line)
        if path ~= nil then
            result_lines[#result_lines + 1] = id .. "\t" .. line
            meta_lines[#meta_lines + 1] = line_number .. "\t" .. path
            id = id + 1
        end
    end

    local results_path = make_temp(join_lines(result_lines))
    local meta_path = make_temp(join_lines(meta_lines))
    if results_path == nil or meta_path == nil then
        remove_files { results_path, meta_path }
        return fail("Failed to prepare grep results")
    end

    return results_path, meta_path
end

local function make_grep_opener(meta_path)
    local script_path = os.tmpname()
    -- Note: 'vicmd' is used verbatim in the shell, so any arguments inside it
    -- (e.g. "vim -g") are interpreted by /bin/sh as usual.
    local vicmd = tostring(vifm.opts.global.vicmd or "vi")
    local script = table.concat({
        "#!/bin/sh",
        "id=$1",
        "entry=$(sed -n \"${id}p\" " .. quote(meta_path) .. ")",
        "line=${entry%%	*}",
        "path=${entry#*	}",
        "[ -n \"$path\" ] || exit 0",
        "exec < /dev/tty > /dev/tty 2>&1",
        vicmd .. " -f +\"$line\" \"$path\"",
        "",
    }, "\n")

    if not write_file(script_path, script) then
        return nil
    end

    run_capture("chmod 700 " .. quote(script_path))
    return script_path
end

function M.grep(info)
    if not executable("fzf") then
        return
    end

    local pattern = info.args ~= nil and info.args ~= "" and info.args or "."
    local root = vifm.currview().cwd
    local results_path, meta_path = prepare_grep_files(root, pattern)
    if results_path == nil then
        return
    end

    local script_path = make_grep_opener(meta_path)
    if script_path == nil then
        remove_files { results_path, meta_path }
        return fail("Failed to create temporary opener for grep results")
    end

    local bind = "enter:execute(" .. quote(script_path) .. " {1})"
    local cmd = "fzf --with-nth=2.. --delimiter=" .. quote("\t") ..
        " --prompt=" .. quote("Grep: ") ..
        " --layout=reverse --height=100% --bind=" .. quote(bind) ..
        " < " .. quote(results_path)

    run_term_capture(cmd)
    remove_files { results_path, meta_path, script_path }
end

local function make_live_grep_cmd(root)
    if vifm.executable("rg") then
        return "rg --line-number --column --with-filename " ..
            "--color=never --smart-case -- \"$q\" " .. quote(root)
    end

    if vifm.executable("grep") then
        return "grep -RInH -I -e \"$q\" " .. quote(root)
    end

    return nil
end

local function make_live_grep_script(root, min_len)
    local grep_cmd = make_live_grep_cmd(root)
    if grep_cmd == nil then
        return nil
    end

    local script_path = os.tmpname()
    local script = table.concat({
        "#!/bin/sh",
        "q=$1",
        "[ ${#q} -lt " .. min_len .. " ] && exit 0",
        "exec " .. grep_cmd,
        "",
    }, "\n")

    if not write_file(script_path, script) then
        return nil
    end

    run_capture("chmod 700 " .. quote(script_path))
    return script_path
end

local function make_path_opener()
    local script_path = os.tmpname()
    -- Note: 'vicmd' is used verbatim in the shell, so any arguments inside it
    -- (e.g. "vim -g") are interpreted by /bin/sh as usual.
    local vicmd = tostring(vifm.opts.global.vicmd or "vi")
    local script = table.concat({
        "#!/bin/sh",
        "path=$1",
        "line=$2",
        "[ -n \"$path\" ] || exit 0",
        "exec < /dev/tty > /dev/tty 2>&1",
        vicmd .. " -f +\"$line\" \"$path\"",
        "",
    }, "\n")

    if not write_file(script_path, script) then
        return nil
    end

    run_capture("chmod 700 " .. quote(script_path))
    return script_path
end

function M.live_grep(info)
    if not executable("fzf") then
        return
    end

    local root = vifm.currview().cwd
    local query = info.args or ""

    local search_script = make_live_grep_script(root, LIVE_GREP_MIN_LEN)
    if search_script == nil then
        return fail("Neither rg nor grep executable found")
    end

    local opener = make_path_opener()
    if opener == nil then
        remove_files { search_script }
        return fail("Failed to create temporary opener for grep results")
    end

    local reload = search_script .. " {q} || true"
    local cmd = table.concat({
        "fzf",
        "--disabled",
        "--ansi",
        "--delimiter=" .. quote(":"),
        "--query=" .. quote(query),
        "--prompt=" .. quote(string.format("LiveGrep(%d+): ", LIVE_GREP_MIN_LEN)),
        "--layout=reverse",
        "--height=100%",
        "--bind=" .. quote("change:reload:" .. reload),
        "--bind=" .. quote("start:reload:" .. reload),
        "--bind=" .. quote("enter:execute(" .. opener .. " {1} {2})"),
    }, " ")

    run_term_capture(cmd)
    remove_files { search_script, opener }
end

local function add_command(name, handler)
    local added = vifm.cmds.add {
        name = name,
        description = "fzf integration",
        handler = handler,
        maxargs = -1,
    }
    if not added then
        vifm.sb.error("Failed to register :" .. name)
    end
end

add_command("FzfCommand", M.command)
add_command("FzfFind", M.find)
add_command("FzfFindStashed", M.find_stashed)
add_command("FzfGrep", M.grep)
add_command("FzfLiveGrep", M.live_grep)

return M
