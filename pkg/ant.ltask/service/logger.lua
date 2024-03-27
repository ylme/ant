local ltask = require "ltask"
local platform = require "bee.platform"

local S = {}

local LOG = (function ()
	if platform.os == 'windows' then
		local windows = require "bee.windows"
		if windows.isatty(io.stdout) then
			return function (_, data)
				windows.write_console(io.stdout, data)
				windows.write_console(io.stdout, "\n")
			end
		end
	end
	if __ANT_RUNTIME__ then
		local ServiceIO = ltask.queryservice "io"
		local directory = require "directory"
		local fs = require "bee.filesystem"
		local logpath = directory.app_path():string()
		local logfile = logpath .. "/game.log"
		fs.create_directories(logpath)
		if fs.exists(logfile) then
			fs.rename(logfile, logpath .. "/game_1.log")
		end
		if platform.os == 'ios' then
			return function (_, data)
				ltask.send(ServiceIO, "SEND", "LOG", data)
				local f <close> = io.open(logfile, "a+")
				if f then
					f:write(data)
					f:write("\n")
				end
				io.write(data)
				io.write("\n")
				io.flush()
			end
		elseif platform.os == 'android' then
			local android = require "android"
			return function (level, data)
				ltask.send(ServiceIO, "SEND", "LOG", data)
				local f <close> = io.open(logfile, "a+")
				if f then
					f:write(data)
					f:write("\n")
				end
				android.rawlog(level, "", data)
			end
		else
			return function (_, data)
				ltask.send(ServiceIO, "SEND", "LOG", data)
				local f <close> = io.open(logfile, "a+")
				if f then
					f:write(data)
					f:write("\n")
				end
			end
		end
	end
	return function (_, data)
		io.write(data)
		io.write("\n")
		io.flush()
	end
end)()

local function writelog()
	while true do
		local ti, _, msg, sz = ltask.poplog()
		if ti == nil then
			break
		end
		local tsec = ti // 100
		local msec = ti % 100
		local level, message = ltask.unpack_remove(msg, sz)
		LOG(level, string.format("[%s.%02d][%-5s]%s", os.date("%Y-%m-%d %H:%M:%S", tsec), msec, level:upper(), message))
	end
end

ltask.fork(function ()
	while true do
		writelog()
		ltask.sleep(100)
	end
end)

function S.quit()
	writelog()
end

return S
