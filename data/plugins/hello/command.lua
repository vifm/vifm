local M = {}

function M.greet(info)
    local what = 'world'
    if info.args ~= '' then
        what = info.args
    end
    vifm.sb.info(string.format('Hello, %s!', what))
end

return M
