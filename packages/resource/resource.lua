local resource = {}

local FILELIST = {}	-- filename -> { filename =, meta = , object =, proxy =, invalid = , source = }
local LOADER = {}
local UNLOADER = {}

-- util functions
local function format_error(format, ...)
	error(format:format(...))
end

local function readonly(self, key)
	format_error("Resource %s is readonly, try to write %s", self, key)
end

local function not_in_memory(self, key)
	if key == nil then
		format_error("Resource %s is not in memory")
	elseif key == "_data" then
		return nil
	else
		format_error("Resource %s is not in memory, try to read %s", self, key)
	end
end

local function data_pairs(self)
	return pairs(self._data)
end

local function data_len(self)
	return #self._data
end

local function data_mt(data, robj)
	return {
		filename = robj.filename,
		__index = data,
		__newindex = readonly,
		__tostring = robj.meta.__tostring,
		__pairs = data_pairs,
		__len = data_len,
	}
end

-- function loader(data) -> table
function resource.register_ext(ext, loader, unloader)
	assert(LOADER[ext] == nil)
	assert(type(loader) == "function")
	LOADER[ext] = loader
	UNLOADER[ext] = unloader
end

local function resolve_path(content)
	local object = { [""] = content }
	local function path_id(prefix, obj)
		for key, value in pairs(obj) do
			if type(value) == "table" then
				local fullkey = prefix .. key
				object[fullkey] = value
				path_id(fullkey .. ".", value)
			end
		end
	end

	path_id("", content)

	return object
end

local function reslove_invalid(robj)
	for path, proxy in pairs(robj.invalid) do
		local data = robj.object[path]
		if data then
			robj.invalid[path] = nil	-- move to proxy set
			if robj.proxy[path] then
				format_error("Duplicate content %s", proxy)
			end
			rawset(proxy, "_data", data)	-- _data is nil, so use rawset
			robj.proxy[path] = setmetatable(proxy, data_mt(data, robj))
		end
	end
end

local function reslove_proxy(robj)
	local object = robj.object
	local proxy_set = robj.proxy

	for path, proxy in pairs(proxy_set) do
		local data = object[path]
		if data then
			proxy._data = data
			setmetatable(proxy, data_mt(data, robj))
		else
			-- can't reslove path
			assert(robj.invalid[path] == nil)
			proxy_set[path] = nil
			proxy._data = nil	-- mark invalid
			robj.invalid[path] = setmetatable(proxy, robj.meta)
		end
	end
	robj.proxy_path = {}
end

local function get_file_object(filename)
	if not FILELIST[filename] then
		-- never load this file
		FILELIST[filename] = {
			filename = filename,
			meta = {
				filename = filename,
				__index = not_in_memory,
				__pairs = not_in_memory,
				__len = not_in_memory,
				__newindex = readonly,
				__tostring = function (self)
					return filename .. ":" .. self._path
				end,
			},
			proxy = {},
			invalid = {},
			object = nil,
		}
	end
	return FILELIST[filename]
end

local function load_resource(robj, filename, data)
	local ext = filename:match "[^.]*$"
	local loader = LOADER[ext]
	if not loader then
		format_error("Unknown ext %s", ext)
	end
	local content = loader(filename, data)
	robj.object = resolve_path(content)
	reslove_invalid(robj)
	reslove_proxy(robj)
end

function resource.load(filename, data, lazyload)
	local robj = get_file_object(filename)
	if lazyload then
		robj.source = data
		-- auto loader
		robj.meta.__index = function (self, key)
			load_resource(robj, robj.filename, robj.source)
			local data = self._data
			if data == nil then
				format_error("%s is invalid", self)
			else
				return data[key]
			end
		end
		robj.meta.__pairs = function (self)
			load_resource(robj, robj.filename, robj.source)
			return pairs(self._data)
		end
		robj.meta.__len = function (self)
			load_resource(robj, robj.filename, robj.source)
			return #self._data
		end
		-- lazy load
		return
	else
		robj.source = nil
		robj.meta.__index = not_in_memory
		robj.meta.__pairs = not_in_memory
		robj.meta.__len = not_in_memory
	end
	if robj.object then
		-- already in memory
		return
	end
	load_resource(robj, filename, data)
end

function resource.unload(filename)
	local robj = FILELIST[filename]
	if robj.object == nil then
		-- not in memory
		return
	end

	local ext = robj.filename:match "[^.]*$"
	if UNLOADER[ext] then
		UNLOADER[ext](robj.object[""], filename, robj.source)
	end

	local meta = robj.meta

	for path, proxy in pairs(robj.proxy) do
		proxy._data = false
		setmetatable(proxy, meta)
	end

	robj.pathid = nil
	robj.object = nil
end

function resource.reload(filename, data)
	local robj = get_file_object(filename)
	if robj.object then
		resource.unload(filename)
	end
	if robj.source then
		robj.source = data
	end
	load_resource(robj, filename, data)
end

local function split_path(fullpath)
	local filename, path = fullpath:match "(.*):(.*)$"
	if filename == nil then
		filename = fullpath
		path = ""
	end
	return filename, path
end

function resource.proxy(fullpath)
	local filename, path = split_path(fullpath)
	local robj = get_file_object(filename)
	local proxy = robj.proxy[path]
	if proxy then
		return proxy
	end
	if robj.object then
		-- in memory
		local data = robj.object[path]
		if data then
			proxy = setmetatable( { _path = path, _data = data }, data_mt(data, robj) )
			robj.proxy[path] = proxy
		elseif robj.invalid[path] then
			return robj.invalid[path]
		else
			-- invalid
			proxy = setmetatable( { _path = path } , robj.meta )
			robj.invalid[path] = proxy
		end
	else
		-- not in memory
		proxy = robj.proxy[path] or robj.invalid[path]
		if not proxy then
			-- create a proxy
			proxy = setmetatable( { _path = path, _data = false }, robj.meta )
			robj.proxy[path] = proxy
		end
	end
	return proxy
end

-- reture "runtime" / "data" / "ref" / "invalid"
-- result : { filenames, ... }
function resource.status(proxy, result)
	local path = proxy._path
	if not path then
		return "runtime"
	end
	local data = proxy._data
	if data == nil then
		return "invalid"
	end
	if data then
		return "data"
	end
	if result then
		local filename = getmetatable(proxy).filename
		if not result[filename] then
			result[filename] = true
			result[#result+1] = filename
		end
	end
	return "ref"
end

-- returns a touched function if enable is true, this function would returns true if the filename is used
function resource.monitor(filename, enable)
	local robj = get_file_object(filename)
	local object = robj.object
	if enable then
		if not object then
			format_error("%s not in memory", filename)
		end
		local touch = false
		for _, proxy in pairs(robj.proxy) do
			local meta = getmetatable(proxy)
			function meta:__index(key)
				touch = true
				return self._data[key]
			end
			function meta:__pairs(key)
				touch = true
				return pairs(self._data)
			end
			function meta:__len(key)
				touch = true
				return #self._data
			end
		end
		return function() return touch end
	elseif object == nil then
		-- already unload
		return
	else
		-- disable
		for _, proxy in pairs(robj.proxy) do
			local meta = getmetatable(proxy)
			meta.__index = proxy._data
			meta.__pairs = data_pairs
			meta.__len = data_len
		end
	end
end

local function apply_patch(obj, patch)
	assert(patch._path == nil)	-- patch must be a normal table
	for k,v in pairs(patch) do
		local original = obj[k]
		if original == nil then
			format_error("the key %s in the patch is not exist in the original object", k)
		end
		if type(original) ~= "table" then
			if type(v) == "table" then
				format_error("patch a none-table key %s with a table", k)
			end
			obj[k] = v
		else
			-- it's sub tree
			if type(v) ~= "table" then
				format_error("patch a sub tree %s with a none-table", k)
			end
			obj[k] = resource.patch(original, v)
		end
	end
end

--it's a local function for apply_patch
local function patch_table(obj, patch)
	local path = obj._path
	if not path then
		-- it's a normal table
		apply_patch(obj, patch)
		return obj
	end
	assert(patch._path == nil)

	local filename = getmetatable(obj).filename
	local prefix
	if path == "" then
		prefix = filename .. ":"
	else
		prefix = filename .. ":" .. path .. "."
	end

	for k,v in pairs(obj) do
		local patch_value = patch[k]
		if patch_value == nil then
			-- clone original value into patch
			if type(v) == "table" and v._path == nil then
				-- a sub tree in resource
				patch[k] = resource.proxy(prefix .. k)
			else
				patch[k] = v
			end
		elseif type(patch_value) == "table" then
			-- patch sub tree
			if type(v) ~= "table" then
				format_error("patch a none-table key %s with a table", k)
			end
			local original_value = v
			if v._path == nil then
				original_value = resource.proxy(prefix .. k)
			end
			patch[k] = patch_table(original_value, patch_value)
		elseif type(v) == "table" then
			format_error("patch a sub tree %s with a none-table", k)
		else
			patch[k] = patch_value
		end
	end

	return patch
end

resource.patch = patch_table

return resource

