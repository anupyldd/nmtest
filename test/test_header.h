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

// inline TestStruct str(
    // "Other file",
    // "test class",
    // []{return Equal(1,2);}
// );

// inline TestStruct a{
    // {
        // .name = "other file",
        // .suite = "test class",
        // .func = []{return Equal(1,2);}
    // }
// };

/*s{
    .suite = "another file",
    .test = "test class",
    .func = []{return Equal(1,2);}
};*/

inline TestS s{
    "another file",
    "test class 1",
    []{return Equal(10,20);}
};


//static auto a = []{Test("TestClass", "from another file").Func([]{return Equal(1,2);});}();
//templates create instances of objects based on type
//what if use this for registering?
//are lambdas different types?
// lambdas ARE different types
////

