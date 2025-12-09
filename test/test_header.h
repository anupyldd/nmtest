#pragma once

#include "names.h"

import nm;

class TestClass
{
public:
    [[nodiscard]]
    auto GetA() const -> int;

private:
    int a = 10;
};

using namespace nm;

inline TestS s{
    names::suite2,
    names::AddLoc("TestClass"),
    []{return Equal(10,20);}
};

template class TestT<
    "Suite 2",
    "!!!TemplateTest (test_source.cpp:18)",
    []{ return Equal(1,2); }>;