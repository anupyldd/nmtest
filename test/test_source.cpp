#include "test_header.h"

auto TestClass::GetA() const -> int
{
    return a;
}

using namespace nm;

inline TestS s2{
    "another file",
    "test class 2",
    []{return Equal(10,20);}
};

template class TestT<
    "AnotherFileTemplate",
    "AnotherFileTemplateTest",
    []{ return Equal(1,2); }>;