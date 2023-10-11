if __ANT_RUNTIME__ then
    error "Cannot be imported in runtime mode."
end

local sha1    = require "sha1"
local config  = require "config"
local depends = require "depends"
local ltask   = require "ltask"

local function get_filename(pathname)
    pathname = pathname:lower()
    local filename = pathname:match "[/]?([^/]*)$"
    return filename.."_"..sha1(pathname)
end

local compiling = {}

local function compile_file(input)
    assert(input:sub(1,1) ~= ".")
    if compiling[input] then
        return ltask.multi_wait(compiling[input])
    end
    compiling[input] = {}
    local ext = input:match "[^/]%.([%w*?_%-]*)$"
    local cfg = config.get(ext)
    local output = cfg.binpath / get_filename(input)
    local changed = depends.dirty(output / ".dep")
    if changed then
        local ok, deps = cfg.compiler(input, output, cfg.setting, changed)
        if not ok then
            local err = deps
            error("compile failed: " .. input .. "\n" .. err)
        end
        depends.insert_front(deps, input)
        depends.writefile(output / ".dep", deps)
    end
    ltask.multi_wakeup(compiling[input], output:string())
    compiling[input] = nil
    return output:string()
end

return {
    set_setting  = config.set,
    compile_file = compile_file,
}
