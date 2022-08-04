set_project("SDF-embree")

add_rules("mode.debug", "mode.release")
set_languages("cxx20")

-- include toolchains
includes("toolchains/*.lua")

if is_os("windows") then
    set_toolchains("clang-cl")
else
    set_toolchains("clang")
end

-- include subprojects
includes("*/xmake.lua")