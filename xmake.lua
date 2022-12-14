set_project("SDF-embree")

add_rules("mode.debug", "mode.release")
set_languages("cxx20")

if is_os("windows") then
    set_toolchains("clang-cl")
    set_toolset("ld", "lld-link.exe")
    set_toolset("sh", "lld-link.exe")
    set_toolset("ar", "lld-link.exe")
    add_defines("_CRT_SECURE_NO_WARNINGS")
else
    set_toolchains("clang")
    add_ldflags("-Wl,-rpath .")
    add_ldflags("-Wl,-rpath $(buildir)")
end

-- include subprojects
includes("*/xmake.lua")