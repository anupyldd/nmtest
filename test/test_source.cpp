#include "test_header.h"

auto TestClass::GetA() const -> int
{
    return a;
}

using namespace nm;

TestS s2{
    .suite = names::suite2,
    .name  = names::AddLoc("TestClass2"),
    .func  = []{return Equal(10,20);},
    .tags  = {names::tag1, names::tag2}
};

template class TestT<
    "Suite 2",
    "TemplateTest (test_source.cpp:18)",
    []{ return Equal(1,2); }>;

template class TestT<
    "Suite 2",
    "!!!TemplateTest (test_source.cpp:18)",
    []{ return Equal(1,2); },
    std::array{"Tag 1", "Tag 2"},
    []{ std::println("setup"); },
    []{ std::println("teardown"); }>;