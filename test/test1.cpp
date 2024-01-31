#include <iostream>
#include <doctest/doctest.h>
using namespace std;

#define JSON(s)                \
    struct s;                  \
    template <typename T>      \
    class sub_class : public T \
    {                          \
    };                         \
    sub_class<s> hx();

JSON(hi)
struct hi
{
    /* data */
};

typedef char ca[2];

char *dx()
{
    return nullptr;
}

// 复杂函数声明
template <typename>
char (&(xxx)(...))[2];

template<>
char (&(xxx<int>)(...))[2]
{

}

auto &ff = xxx<int>();

constexpr auto xs = sizeof(xxx<int>(0));

// 成员限定名查找

struct has_init
{
    void init() {}
};

struct has_static_init
{
    static void init() {}
};

struct some_has: has_static_init,has_init
{
    // void init() {}
};

//查找有歧义
// void (has_init::* p)()=&some_has::init;

// int main()
// {
//     auto v = sub_class<hi>();
//     // hx=v;
//     return 0;
// }