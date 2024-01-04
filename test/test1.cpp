#include <iostream>

using namespace std;

#define JSON(s)\
struct s; \
template<typename T> \
class sub_class : public T \
{ \
};\
sub_class<s>  hx(); 

JSON(hi)
struct hi
{
    /* data */
};


int main()
{
    auto v=sub_class<hi>();
    // hx=v;
    return 0;
}