#include <regex>
#include <string>
#include <iostream>
#include <vector>

#include <shared_mutex>
#include <mutex>
#include <set>

#include "untrust/kmp.h"

#include "doctest/doctest.h"

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

struct block
{
    int begin_;
    int size_;
};

bool operator<(const block &fk, int lk) { return !(fk.begin_ + fk.size_ > lk); }
bool operator<(int lk, const block &fk) { return lk < fk.begin_; }
bool operator<(const block &fk1, const block &fk2) { return fk1.begin_ < fk2.begin_; }

void set_test()
{
    std::set<block, std::less<>> c = {{1, 10}, {11, 30}, {41, 5}, {46, 5}};
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

namespace regex_test
{
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
    using namespace std;
    inline std::regex hex_reg(R"(^[1-9A-Fa-f][0-9A-Fa-f]{0,15})");

    inline std::regex zero_reg(R"(^0{0,15})");
    void zero_reg_test(string str)
    {
        std::smatch m;
        if (!std::regex_search(str, m, zero_reg))
        {
            cout << "匹配不正确" << endl;
            return;
        }
        auto l = std::stoul(m.str(), nullptr, 16);
        return;
    }

    void hex()
    {
        std::regex hex_reg2(R"(^0{1,15}|[1-9A-Fa-f][0-9A-Fa-f]{0,15})");
        auto check = [&](std::string text)
        {
            std::smatch m;
            if (!std::regex_search(text, m, hex_reg2))
            {
                cout << "匹配不正确" << endl;
                return;
            }
            auto l = std::stoul(m[0], nullptr, 16);
        };
        check("d33");
        return;
    }

} // namespace regex_test

namespace kmp_test
{
    void test()
    {
        string pattern = "AB00";

        auto check = [&](std::string text)
        {
            hcpp::untrust::KMP kmp(pattern);
            auto result = kmp.search(text);
            if (result == -1)
            {
                cout << "Pattern not found in the text" << result << endl;
            }
            else if (text.size() - result == pattern.size())
            {
                cout << "Pattern found at index " << result << endl;
            }
            else
            {
                cout << "Pattern not found in the text, but last match at index " << result << endl;
            }
        };
        check("ABABDABACDABABCABAB");
        check("");
        check("AB00");
    }
} // namespace kmp_test

TEST_CASE("kmp_test")
{
    // kmp_test::test();

    hcpp::untrust::KMP kmp("abc");
    CHECK(kmp.search("aabab") == 3);
    CHECK(kmp.search("c") == 3);

    // dcdax 匹配 abcdcdaxdd
    hcpp::untrust::KMP kmp2("dcdax");
    CHECK(kmp2.search("abcdcd") == 3);
    CHECK(kmp2.search("axdd") == 3);

    {
        // dcdax 匹配 abcdddcdax
        hcpp::untrust::KMP kmp("dcdax");
        CHECK(kmp.search("abcdd") == 4);
        CHECK(kmp.search("dcd") == 5);
        CHECK(kmp.search("ax") == 5);
    }
}

// int main(int argc, char const *argv[])
// {
//     // lock_test();
//     // set_test();
//     // cout << zero_reg_test("0000000") << endl;
//     // kmp_test::test();
//     regex_test::hex();
//     return 0;
// }

TEST_CASE("example")
{
    std::regex hex_reg2(R"(^0{1,15}|[1-9A-Fa-f][0-9A-Fa-f]{0,15})");
    auto check = [&](std::string text)
    {
        std::smatch m;
        if (!std::regex_search(text, m, hex_reg2))
        {
            cout << "匹配不正确" << endl;
            CHECK(false);
        }
        auto l = std::stoul(m[0], nullptr, 16);
    };
    check("d33");
}