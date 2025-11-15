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
    assert(res == true);
}

constexpr auto AssertFailure(const Result& res) noexcept -> void
{
    assert(res == false);
}

void TestLib()
{
    std::println("--- [nm] testing started");

    std::println("------ [nm] assert testing started");

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

    std::println("------ [nm] assert testing finished successfully");
    std::println("------ [nm] registry testing started");

    {
        Registry regs;
        const auto& suites = regs.Suites();

        assert(suites.size() == 0);

        regs.Suite("Suite 1")
            .Setup([]{ std::println("Suite 1 setup"); })
            .Teardown([]{ std::println("Suite 1 teardown"); })
            .Add(
                Test("Test 1.1")
                .Setup([]{ std::println("Test 1.1 setup"); })
                .Teardown([]{ std::println("Test 1.1 teardown"); })
                .Func([]
                {
                    return Report{}
                    & Equal(1, 1)
                    & Equal(1, 2);
                }),

                Test("Test 1.2")
                .Setup([]{ std::println("Test 1.2 setup"); })
                .Teardown([]{ std::println("Test 1.2 teardown"); })
                .Func([]
                {
                    Report rep;
                    rep & Equal(1, 1)
                        & Equal(1, 2);
                    return rep;
                })
            );

        assert(suites.size() == 1);
        assert(suites.at(0).Name() == "Suite 1");
        assert(suites.at(0).Setup());
        assert(suites.at(0).Teardown());
        assert(suites.at(0).Tests().size() == 2);

        const auto& t1 = regs.Suites().at(0).Tests().at(0);
        const auto& t2 = regs.Suites().at(0).Tests().at(1);

        assert(t1.Name() == "Test 1.1");
        assert(t1.Setup());
        assert(t1.Teardown());
        assert(t1.Func());

        assert(t2.Name() == "Test 1.2");
        assert(t2.Setup());
        assert(t2.Teardown());
        assert(t2.Func());

        std::println("--------- [nm] expected: 2 failed asserts, 2 failed tests");
        regs.Run();
    }

    std::println("--- [nm] testing finished successfully");
}
