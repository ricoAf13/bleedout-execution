-- set minimum xmake version
set_xmakever("2.8.2")

-- includes
includes("extern/commonlibsse-ng")

-- set project
set_project("BleedoutExecution")
set_version("1.0.0")
set_license("MIT")

-- set defaults
set_languages("c++23")
set_warnings("allextra", "error")
set_defaultmode("releasedbg")

-- add rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- set policies
set_policy("package.requires_lock", true)

-- set runtimes
set_runtimes("MD")

-- set encodings
set_encodings("utf-8")
set_encodings("source:utf-8", "target:utf-8")

-- set configs
set_config("skyrim_se", true)
set_config("skyrim_ae", true)
set_config("skyrim_vr", false)

-- set requires
add_requires("spdlog", { configs = { header_only = false, wchar = true, std_format = true } })
add_requires("nlohmann_json")

-- targets
target("BleedoutExecution")
    set_kind("shared")

    add_cxxflags(
        "cl::/wd4200", -- zero-sized array in struct/union
        "cl::/wd4201"  -- nameless struct/union
    )

    -- add dependencies to target
    add_deps("commonlibsse-ng")
    -- add commonlibsse-ng plugin
    add_rules("commonlibsse-ng.plugin", {
        name = "BleedoutExecution",
        author = "User",
        description = "Post-Hit Bleedout Execution - plays killmove animations on bleedout NPCs."
    })
    
    add_packages("spdlog", "nlohmann_json")

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    
    add_includedirs("include")
    add_headerfiles("include/**.h")
    set_pcxxheader("include/PCH.h")
