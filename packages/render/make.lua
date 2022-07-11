local lm = require "luamake"

lm:lua_source "render" {
    includes = {
        "../../3rd/bgfx/include",
        "../../3rd/bx/include",
        "../../clibs/bgfx",
        "../../clibs/lua",
        "../../clibs/math3d",
        "../../clibs/foundation",
        "../../3rd/luaecs",
        "../../clibs/ecs",
    },
    sources = {
        "material/material.c",
        --"mesh/mesh.c",
        "cull.cpp",
    },
}