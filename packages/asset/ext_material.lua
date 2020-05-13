local ecs = ...

local math3d = require "math3d"
local bgfx = require "bgfx"
local assetmgr 	= require "asset"

local m = ecs.component "uniform"

local function uniform_data(v)
	local num = #v
	if num == 4 then
		return math3d.ref(math3d.vector(v))
	elseif num == 16 then
		return math3d.ref(math3d.matrix(v))
	elseif num == 0 then
		return math3d.ref()
	else
		error(string.format("invalid uniform data, only support 4/16 as vector/matrix:%d", num))
	end
end

function m:init()
	local input = self.value or self
	for i = 1, #input do
		self[i] = uniform_data(input[i])
	end
	return self
end

function m:save()
    return math3d.totable(self)
end

local m = ecs.component "mat_texture"

function m:init()
	self.handle = self.texture.handle
	return self
end

local m = ecs.component "material"

function m:init()
	assert(type(self.fx) ~= "string")
	if type(self.state) == "string" then
		self.state = bgfx.make_state(assetmgr.load_component(world, self.state))
	else
		self.state = bgfx.make_state(self.state)
	end
	return self
end
