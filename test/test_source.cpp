#include "test_header.h"

auto TestClass::GetA() const -> int
{
    return a;
}

using namespace nm;

inline TestS s2{
    names::suite2,
    names::AddLoc("TestClass2"),
    []{return Equal(10,20);},
    {names::tag1, names::tag2}
};

template class TestT<
    "Suite 2",
    "TemplateTest (test_source.cpp:18)",
    []{ return Equal(1,2); }>;