set_project("SDF-embree")

add_rules("mode.debug", "mode.release")
set_languages("cxx20")

-- include toolchains
includes("toolchains/*.lua")
set_toolchains("clang-cl")

-- include subprojects
includes("*/xmake.lua")