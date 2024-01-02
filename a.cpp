#include <regex>
#include <string>
#include <iostream>

using namespace std;

int main(int argc, char const *argv[])
{
    std::string xx{33,'a'};
    auto svl = "aaa.ccc.dddx..:666";
    std::regex host_reg(R"([\w\.]+(:(0|[1-9]\d{0,4}))?)");
    std::cmatch m;
    if (!std::regex_match(svl, m, host_reg))
    {
        return -1;
    }
     std::cout << m.size() << std::endl;
    for (int i = 0; auto &&sm : m)
    {
        std::cout << sm.str() << std::endl;
    }
    std::string ll="abc\r\n";
    cout<<ll.find("\r\n",0,2)<<endl;

    std::string const s = "This is a string"; 
    return 0;
}
