#include <regex>
#include <string>
#include <iostream>

#include <shared_mutex>
#include <mutex>
#include <set>

using namespace std;

void lock_test()
{
    static std::shared_mutex smutex_;
    // std::shared_lock<std::shared_mutex> m(smutex_);
    {
        std::shared_lock<std::shared_mutex> m2(smutex_);
        std::shared_lock<std::shared_mutex> m3(smutex_);
        std::shared_lock<std::shared_mutex> m4(smutex_);
    } // ok

    {
        std::shared_lock<std::shared_mutex> m3(smutex_);
        // 阻塞,一直等待共享锁释放
        std::unique_lock<std::shared_mutex> m2(smutex_);
    }
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

struct block
{
    int begin_;
    int size_;
};

bool operator<(const block &fk, int lk) { return  !(fk.begin_ + fk.size_>lk); }
bool operator<(int lk, const block &fk) { return lk < fk.begin_; }
bool operator<(const block &fk1, const block &fk2) { return fk1.begin_ < fk2.begin_; }

void set_test()
{
    std::set<block, std::less<>> c = {{1, 10}, {11, 30}, {41, 5},{46, 5}};
    auto pf = [&c](int i)
    {
        if (auto search = c.find(i); search != c.end())
            std::cout << "Found " << search->begin_ << '\n';
        else
            std::cout << "Not found\n";
    };
    pf(1);
    pf(11);
    pf(30);

    pf(41);
    pf(46);
    pf(51);
}

int main(int argc, char const *argv[])
{
    // lock_test();
    set_test();
    return 0;
}
