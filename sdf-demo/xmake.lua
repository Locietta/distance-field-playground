add_requires("fmt", "embree", "glm")

target("sdf-demo")
    set_kind("binary")
    add_files("src/*.cpp")
    add_includedirs("include")
    add_packages("fmt", "embree", "glm")
    