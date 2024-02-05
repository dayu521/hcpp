set_project("hcpp")
set_xmakever("2.8.6")
add_rules("mode.debug", "mode.release")

if is_os("linux") then
    set_allowedmodes("debug")
    set_defaultmode("debug")
    set_toolchains("clang")
elseif is_os("windows") then
    set_encodings("utf-8")
    -- add_cxxflags("/source-charset:utf-8")
else

end

set_warnings("all")

set_languages("c++20")

-- 设置代理镜像
-- xmake g --proxy_pac=github_mirror.lua
-- https://xmake.io/#/zh-cn/package/remote_package?id=%e4%bd%bf%e7%94%a8%e8%87%aa%e5%bb%ba%e7%a7%81%e6%9c%89%e5%8c%85%e4%bb%93%e5%ba%93
package("lsf")
    set_homepage("https://gitee.com/californiacat/lsf.git")
    set_description("json.")
    set_urls("https://gitee.com/californiacat/lsf.git")
    -- set_sourcedir(path.join(os.scriptdir(), "lib/lsf"))
    
    on_install(function (package)
        os.cp("src/lsf", package:installdir("include"))
        -- os.cp("libjpeg.lib", package:installdir("lib"))
        import("package.tools.xmake").install(package,{})
    end)
package_end()

local openssl_package_name = ""  
if is_os("windows") then
    add_requires("asio 1.28.0",{verify = false})
    add_requires("openssl3",{verify = false})
    add_requires("lsf")
    openssl_package_name = "openssl3"
else
    add_requires("asio >=1.28.0",{verify = false})
    add_requires("openssl >=3.2.0",{verify = false})
    add_requires("lsf" ,{debug = true})
    openssl_package_name = "openssl"
end

add_requires("spdlog >=1.12.0")

target("hcpp")
    -- add_ldflags("-static")
    add_packages("spdlog",{public = true})
    add_packages("asio",{public = true}) 
    add_packages(openssl_package_name,{public = true})
    add_packages("lsf",{public = true}) 

    add_includedirs("src",{public = true})
    add_files("src/*.cpp","src/https/*.cpp","src/http/*.cpp","src/dns/*.cc")
    add_files("main.cpp")

    set_policy("build.c++.modules", true)
    
    -- set_rundir("$(buildir)")
    -- add_configfiles("src/hcpp-cfg.json")
    after_build(function (target)
        os.cp("src/hcpp-cfg.json", target:targetdir())
        os.cp("doc/cert/server.crt.pem", target:targetdir())
        os.cp("doc/cert/server.key.pem", target:targetdir())
    end)
    -- before_link(function (target)
        -- add_linkdirs("src/build/linux/x86_64/debug")
        -- add_links("hcpp_imp")
    -- end)
    -- before_run(function (target)
    --     os.cp("src/hcpp-cfg.json", target:targetdir())
    -- end)

    -- for _, testfile in ipairs(os.files("test/**.cpp")) do
    --     add_tests(path.basename(testfile), {
    --         kind = "binary",
    --         files = testfile,
    --         packages = "doctest",
    --         remove_files="main.cpp",
    --         defines = "DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN"})
    -- end
target_end()

add_requires("doctest")

target("a_test")
    add_packages("doctest")
    add_includedirs("src")
    set_default("false")
    add_files("test/src/a.cpp",{defines = "DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN"})
    add_tests("a_test")

-- 打包源码
-- includes("@builtin/xpack")
-- xpack("hcpp")
--     set_formats("srczip", "srctargz")
--     add_sourcefiles("(./**)")

-- includes("test")
