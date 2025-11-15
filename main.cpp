#include <cassert>
#include <print>

import nm;

void TestLib();

int main()
{
    TestLib();
}

using namespace nm;

constexpr auto AssertSuccess(const Result& res) noexcept -> void
{
    return assert(res == true);
}

constexpr auto AssertFailure(const Result& res) noexcept -> void
{
    return assert(res == false);
}

void TestLib()
{
    std::println("nmtest testing started");

    AssertSuccess(Equal(1, 1));
    AssertFailure(Equal(1, 2));
    AssertSuccess(Equal(1.1, 1.1));

    AssertFailure(NotEqual(1, 1));
    AssertSuccess(NotEqual(1, 2));
    AssertSuccess(NotEqual(1.12, 1.1));

    AssertSuccess(True(true));
    AssertFailure(True(false));

    AssertFailure(False(true));
    AssertSuccess(False(false));

    {
        constexpr auto a = 10;
        const auto ptr = &a;

        AssertSuccess(Null(nullptr));
        AssertFailure(Null(ptr));

        AssertSuccess(NotNull(ptr));
        AssertFailure(NotNull(nullptr));
    }

    {
        Registry regs;

    }

    std::println("nmtest testing finished successfully");
}
