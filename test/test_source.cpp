#include "test_header.h"

auto TestClass::GetA() const -> int
{
    return a;
}

using namespace nm;

TestS s2{
    names::suite2,
    names::AddLoc("TestClass2"),
    []{return Equal(10,20);},
    {names::tag1, names::tag2}
};

template class TestT<
    "Suite 2",
    "TemplateTest (test_source.cpp:18)",
    []{ return Equal(1,2); }>;

TestSD s3{
    {
        .suite = names::suite2,
        .name  = names::AddLoc("SD"),
        .tags  = { "tag 1", "tag 2" },
        .func  = []{ return Equal(0,2); }
    }
};