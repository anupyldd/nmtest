#pragma once

#include <print>
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
    "another file",
    "test class 1",
    []{return Equal(10,20);}
};
