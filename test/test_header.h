#pragma once

import nm;

class TestClass
{
public:
    [[nodiscard]]
    auto GetA() const -> int;

private:
    int a = 10;
};

