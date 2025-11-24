#include "test_header.h"

auto TestClass::GetA() const -> int
{
    return a;
}


static nm::TestStruct b{
        {
            .name = "other file 2",
            .suite = "test class 2",
            .func = []{return nm::Equal(10,2);}
        }
};
