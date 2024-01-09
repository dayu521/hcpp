#include <regex>
#include <string>
#include <iostream>

#include <shared_mutex>
#include <mutex>

using namespace std;

void lock_test()
{
    static std::shared_mutex smutex_;
    // std::shared_lock<std::shared_mutex> m(smutex_);
    {
        std::shared_lock<std::shared_mutex> m2(smutex_);
        std::shared_lock<std::shared_mutex> m3(smutex_);
        std::shared_lock<std::shared_mutex> m4(smutex_);
    } //ok

    {
        std::shared_lock<std::shared_mutex> m3(smutex_);
        std::unique_lock<std::shared_mutex> m2(smutex_);
    }//一直等待共享锁释放
}

void regex_test()
{
    std::string xx{33, 'a'};
    auto svl = "aaa.ccc.dddx..:666";
    std::regex host_reg(R"([\w\.]+(:(0|[1-9]\d{0,4}))?)");
    std::cmatch m;
    if (!std::regex_match(svl, m, host_reg))
    {
        return;
    }
    std::cout << m.size() << std::endl;
    for (int i = 0; auto &&sm : m)
    {
        std::cout << sm.str() << std::endl;
    }
    std::string ll = "abc\r\n";
    cout << ll.find("\r\n", 0, 2) << endl;

    std::string const s = "This is a string";
}

int main(int argc, char const *argv[])
{
    lock_test();
    return 0;
}
