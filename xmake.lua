add_rules("mode.debug", "mode.release")

if is_os("linux") then
    set_allowedmodes("debug")
    set_defaultmode("debug")
    set_toolchains("clang")
elseif is_os("windows") then
    -- set_encodings("utf-8")
    add_cxxflags("/source-charset:utf-8")
else

end

set_warnings("all")

set_languages("c++20")

-- 设置代理镜像
-- xmake g --proxy_pac=github_mirror.lua
-- https://gitee.com/californiacat/lsf.git
-- https://xmake.io/#/zh-cn/package/remote_package?id=%e4%bd%bf%e7%94%a8%e8%87%aa%e5%bb%ba%e7%a7%81%e6%9c%89%e5%8c%85%e4%bb%93%e5%ba%93
package("lsf")
    set_homepage("https://gitee.com/californiacat/lsf.git")
    set_description("json.")
    set_urls("https://gitee.com/californiacat/lsf.git")
    -- set_sourcedir(path.join(os.scriptdir(), "lib/lsf"))
    
    on_install(function (package)
        os.cp("src/public/*.h", package:installdir("include"))
        -- os.cp("libjpeg.lib", package:installdir("lib"))
        import("package.tools.xmake").install(package)
    end)
package_end()

-- includes("lib/lsf")

if is_os("windows") then
    add_requires("asio 1.28.0",{verify = false})
    add_requires("openssl3",{verify = false})
else
    add_requires("asio >=1.28.0",{verify = false})
    add_requires("openssl >=3.2.0",{verify = false})
end

add_requires("spdlog >=1.12.0")
add_requires("lsf" ,{debug = true})

target("hcpp")
    set_kind("binary")
    -- add_ldflags("-static")
    add_includedirs("src")
    add_files("main.cpp","src/*.cpp","src/https/*.cpp","src/http/*.cpp","src/dns/*.cc")
    -- add_deps("lsf") --  https://xmake.io/#/manual/project_target?id=add-target-dependencies
    add_packages("spdlog")  --  https://xmake.io/#/manual/project_target?id=add-package-dependencies
    add_packages("asio") 
    if is_os("windows") then 
        add_packages("openssl3") 
    else
        add_packages("openssl") 
    end 
    add_packages("lsf") 

    set_policy("build.c++.modules", true)
    -- set_rundir("$(buildir)")
    -- add_configfiles("src/hcpp-cfg.json")

    after_build(function (target)
        os.cp("src/hcpp-cfg.json", target:targetdir())
        os.cp("doc/cert/server.crt.pem", target:targetdir())
        os.cp("doc/cert/server.key.pem", target:targetdir())
    end)
    -- before_run(function (target)
    --     os.cp("src/hcpp-cfg.json", target:targetdir())
    -- end)
target_end()

-- includes("@builtin/xpack")
-- xpack("hcpp")
--     set_formats("srczip", "srctargz")
--     add_sourcefiles("(./**)")

-- includes("test")

-- https://zhuanlan.zhihu.com/p/640701847
-- https://zhuanlan.zhihu.com/p/479977993
