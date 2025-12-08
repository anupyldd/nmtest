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
