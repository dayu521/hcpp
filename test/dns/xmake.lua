add_rules("mode.debug", "mode.release")

add_requires("doctest")
add_requires("openssl")

set_languages("c++20")
if is_os("windows") then
    add_defines("MSVC_SPECIAL")
    add_cxxflags("/source-charset:utf-8")
end

target("dns_t")
    set_kind("binary")
    add_includedirs("../../src/dns/")
    add_includedirs("../../src/")
    add_files("./*.cpp","../../src/dns/*.cc")
    add_packages("doctest")
    add_packages("openssl") 
