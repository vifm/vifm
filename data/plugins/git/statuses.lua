local M = {
    show_unchanged = true,
}

local cp_handle = io.popen('luarocks --lua-version=5.4 path --lr-cpath')
local lua_cpath = cp_handle:read()
cp_handle:close()
if lua_cpath ~= '' then
    package.cpath = lua_cpath .. ';' .. package.cpath
end

local cffi = require('cffi')
local git = cffi.load('libgit2.so')

cffi.cdef([[
    typedef struct git_repository git_repository;
    int git_libgit2_init();
    void git_repository_free(git_repository *repo);

    int git_repository_open_ext(git_repository **out, const char *path, unsigned int flags, const char *ceiling_dirs);
    const char *git_repository_workdir(const git_repository *repo);

    typedef int (*git_status_cb)(const char *path, unsigned int status_flags, void *payload);
    int git_status_foreach(git_repository *repo, git_status_cb callback, void *payload);

    typedef struct git_object git_object;
    int git_revparse_single(git_object **out, git_repository *repo, const char *spec);
    void git_object_free(git_object *object);

    typedef struct git_commit git_commit;
    typedef struct git_tree_entry git_tree_entry;
    typedef struct git_tree git_tree;
    typedef enum {
        GIT_TREEWALK_PRE = 0, /* Pre-order */
        GIT_TREEWALK_POST = 1 /* Post-order */
    } git_treewalk_mode;
    typedef int (*git_treewalk_cb)(const char *root, const git_tree_entry *entry, void *payload);
    int git_tree_walk(const git_tree *tree, git_treewalk_mode mode, git_treewalk_cb callback, void *payload);
    void git_tree_free(git_tree *tree);
    const char * git_tree_entry_name(const git_tree_entry *entry);
    typedef int git_object_t;
    git_object_t git_tree_entry_type(const git_tree_entry *entry);
]])
git.git_libgit2_init()

-- a user may be making changes in a repository quite frequently
local GIT_DIR_TTL = 5
-- repositories are created infrequently
local NON_GIT_DIR_TTL = 60

-- root node of the cache, initialized by M.reset()
local cache

local function find_node(path)
    local current = cache

    for entry in string.gmatch(path, '[^/]+') do
        local next = current.subs[entry]
        if next == nil then
            return nil
        end

        current = next
    end

    return current
end

local function init_node(node, expires)
    node.subs = {}
    node.items = {}
    node.expires = expires
    node.status = nil
    return node
end

local function make_node(node, path)
    local current = node

    for entry in string.gmatch(path, '[^/]+') do
        local next = current.subs[entry]
        if next == nil then
            next = init_node({}, 0)
            current.subs[entry] = next
        end

        current = next
    end

    return current
end

local function combine_status(a, b)
    if a == b then
        return a
    end
    if a == ' ' or a == '!' then
        return b
    end
    if b == ' ' or b == '!' then
        return a
    end
    if a == 'G' then
        return b
    end
    if b == 'G' then
        return a
    end
    return 'X'
end

local function update_dir_status(node, status)
    if node.status == nil then
        node.status = status
    else
        local staged = combine_status(node.status:sub(1, 1), status:sub(1, 1))
        local index = combine_status(node.status:sub(2, 2), status:sub(2, 2))
        node.status = staged..index
    end
end

local function set_file_status(node, path, status, expires)
    local slash = path:find('/')
    if slash == nil then
        -- a file removed from index appears twice: first as  'D ' then as '??',
        -- keep the first status
        if node.items[path] == nil or node.items[path] == 'GG' then
            node.items[path] = status
            update_dir_status(node, status)
        end
        return node.status
    end

    local entry = path:sub(1, slash - 1)
    path = path:sub(slash + 1)

    local next = node.subs[entry]
    if next == nil then
        next = init_node({}, expires)
        next.in_git = true
        node.subs[entry] = next
    end

    if path ~= '' then
        status = set_file_status(next, path, status, expires)
    end

    update_dir_status(node, status)
    return node.status
end

function redraw()
    vifm.opts.global.laststatus = not vifm.opts.global.laststatus
    vifm.opts.global.laststatus = not vifm.opts.global.laststatus
end

local STATUS_FLAGS = {
    CURRENT = 0,

    INDEX_NEW = 0x1,
    INDEX_MODIFIED = 0x2,
    INDEX_DELETED = 0x4,
    INDEX_RENAMED = 0x8,
    INDEX_TYPECHANGE = 0x10,

    WT_NEW = 0x80,
    WT_MODIFIED = 0x100,
    WT_DELETED = 0x200,
    WT_TYPECHANGE = 0x400,
    WT_RENAMED = 0x800,
    WT_UNREADABLE = 0x1000,

    IGNORED = 0x4000,
    CONFLICTED = 0x8000,
}

local function map_status(flags)
    local i, w = ' ', ' '
    if flags & STATUS_FLAGS.INDEX_NEW ~= 0 then i = 'A'
    elseif flags & STATUS_FLAGS.INDEX_MODIFIED ~= 0 then i = 'M'
    elseif flags & STATUS_FLAGS.INDEX_DELETED ~= 0 then i = 'D'
    elseif flags & STATUS_FLAGS.INDEX_RENAMED ~= 0 then i = 'R'
    elseif flags & STATUS_FLAGS.INDEX_TYPECHANGE ~= 0 then i = 'T'
    end
    if flags & STATUS_FLAGS.WT_NEW ~= 0 then return '??'
    elseif flags & STATUS_FLAGS.WT_MODIFIED ~= 0 then w = 'M'
    elseif flags & STATUS_FLAGS.WT_DELETED ~= 0 then w = 'D'
    elseif flags & STATUS_FLAGS.WT_RENAMED ~= 0 then w = 'R'
    elseif flags & STATUS_FLAGS.WT_TYPECHANGE ~= 0 then w = 'T'
    end
    if i == ' ' and w == ' ' and flags & STATUS_FLAGS.IGNORED ~= 0 then
        return '!!'
    end
    return i .. w
end

function update_subdir(sub_at, path, node)
    local repo_ptr = cffi.new('git_repository*[1]')
    if git.git_repository_open_ext(repo_ptr, sub_at, 0, nil) ~= 0 then
        return
    end
    local repo = repo_ptr[0]
    node.items[path] = 'GG'
    local status_cb = cffi.cast('git_status_cb', function(_rel_path, flags, _payload)
        if flags == 0 or flags == STATUS_FLAGS.IGNORED then
            return 0
        end
        node.items[path] = map_status(flags)
        return 1
    end)
    git.git_status_foreach(repo, status_cb, nil)
    git.git_repository_free(repo)
    redraw()
end

--- Check the status of git repository subdirectories
function update_subdirs(at, node)
    local cmd = string.format('find %s -mindepth 2 -maxdepth 2 \\( -type d -o -type f \\) -name .git -print0',
                              vifm.escape(at))
    vifm.startjob {
        cmd = cmd,
        onexit = function(job)
            local status_all = job:stdout():read('a')
            node.has_repos = status_all ~= ''
            for entry in string.gmatch(status_all, '[^\0]+') do
                local sub_at = vifm.fnamemodify(entry, ':h')
                local sub_root = vifm.fnamemodify(entry, ':h:t')
                update_subdir(sub_at, sub_root, node)
            end
        end
    }
end

function is_dir(path)
    local f = io.open(path .. "/")
    if f then
        f:close()
        return true
    end
    return false
end

local function shallow(tbl)
    if tbl == nil then
        return nil
    end

    local copy = { }
    for k, v in pairs(tbl) do
        copy[k] = v
    end
    return copy
end

local function fill_node(node, cb, ...)
    node.pending = (node.pending or 0) + 1
    coroutine.resume(coroutine.create(function(...)
        cb(...)
        node.pending = node.pending - 1
        if node.pending == 0 then
            -- Can drop the cache now.
            node.past = nil
            redraw()
        end
    end), ...)
end

function update_status(at, root, node, expires)
    local repo_ptr = cffi.new('git_repository*[1]')
    if git.git_repository_open_ext(repo_ptr, at, 0, nil) ~= 0 then
        return
    end
    local repo = repo_ptr[0]
    local status_cb = cffi.cast('git_status_cb', function(rel_path, flags, _payload)
        rel_path = cffi.string(rel_path)
        local abs_path = root..rel_path
        if rel_path:sub(-1) == '/' and is_dir(abs_path..'.git') then
            -- Strip the end of `rel_path`, to say the contents of the
            -- sub-repo isn't cached.
            update_subdir(abs_path, rel_path:sub(1, -2), node)
        else
            set_file_status(node, rel_path, map_status(flags), expires)
        end
        return 0
    end)
    git.git_status_foreach(repo, status_cb, nil)
    git.git_repository_free(repo)
end

GIT_TREEWALK_PRE = 0
GIT_OBJECT_BLOB = 3

function update_unchanged(at, node, expires)
    local repo_ptr = cffi.new('git_repository*[1]')
    if git.git_repository_open_ext(repo_ptr, at, 0, nil) ~= 0 then
        return
    end
    local obj_ptr = cffi.new('git_object*[1]')
    local repo = repo_ptr[0]
    if git.git_revparse_single(obj_ptr, repo, 'HEAD^{tree}') == 0 then
        local tree = cffi.cast('git_tree*', obj_ptr[0])
        local walk_cb = cffi.cast('git_treewalk_cb', function(dir, entry, _payload)
            if git.git_tree_entry_type(entry) ~= GIT_OBJECT_BLOB then
                return 0
            end
            local name = cffi.string(git.git_tree_entry_name(entry))
            local path = cffi.string(dir) .. name
            set_file_status(node, path, 'GG', expires)
            return 0
        end)
        git.git_tree_walk(tree, GIT_TREEWALK_PRE, walk_cb, nil)
        git.git_tree_free(tree)
        git.git_object_free(obj_ptr[0])
    end
    git.git_repository_free(repo)
end

function M.get(at)
    at = vifm.fnamemodify(at, ':p')
    if vifm.fnamemodify(at, ':t') == '.' then
        at = vifm.fnamemodify(at, ':h')
    end

    local cached = find_node(at)
    if cached ~= nil then
        if cached.expires >= os.time() then
            -- Return old cached data while new one is being retrieved to avoid
            -- flickering.
            -- XXX: this doesn't apply to nested paths.
            return cached.past or cached
        end
    end

    local repo_ptr = cffi.new('git_repository*[1]')
    local ts = os.time()
    local err = git.git_repository_open_ext(repo_ptr, at, 0, nil)
    local repo, root_ptr
    if err == 0 then
        repo = repo_ptr[0]
        root_ptr = git.git_repository_workdir(repo)
    end
    if err ~= 0 or root_ptr == cffi.nullptr then
        -- Handle directories outside of git repositories and avoid clearing
        -- accumulated cache of its children.
        local node = make_node(cache, at)
        node.expires = ts + NON_GIT_DIR_TTL
        node.in_git = false
        update_subdirs(at, node)
        return node
    end
    local root = cffi.string(root_ptr)
    git.git_repository_free(repo)

    -- Make a copy before invoking init_node() on the same node.
    cached = shallow(cached)

    local expires = ts + GIT_DIR_TTL
    local node = init_node(make_node(cache, at), expires)
    node.in_git = true
    node.past = cached
    fill_node(node, update_status, at, root, node, expires)
    if M.show_unchanged then
        fill_node(node, update_unchanged, at, node, expires)
    end
    return node.past or node
end

function M.reset()
    cache = {
        in_git = false,    -- whether current path is covered by Git
        has_repos = false, -- whether current path is covered by Git
        status = nil,      -- status derived from nested files
        subs = { },        -- subdirectory name -> cache node
        items = { },       -- file name         -> status
        expires = 0        -- when the cache entry expires
    }

    redraw()
end

M.reset()

return M
