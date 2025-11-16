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

    std::println("--- --- [nm] assert testing started");

    AssertSuccess(Equal(1.0f, 1.0f));
    AssertSuccess(Equal(1.0, 1.0));
    AssertSuccess(Equal(0.0f, 0.0f));
    AssertSuccess(Equal(0.0, 0.0));

    AssertFailure(Equal(1.0f, 2.0f));
    AssertFailure(Equal(1.0, 2.0));

    // small representable differences
    AssertSuccess(Equal(1.0f, 1.0f + 1e-6f));
    AssertFailure(Equal(1.0, 1.0 + 1e-12));

    // slightly too large differences
    AssertFailure(Equal(1.0f, 1.0f + 1e-3f));
    AssertFailure(Equal(1.0, 1.0 + 1e-8));

    // very small numbers
    AssertSuccess(Equal(1e-40f, 2e-40f));
    AssertSuccess(Equal(1e-320, 2e-320));
    AssertFailure(Equal(1e-40f, 1e-30f));

    // opposite signs but near zero
    AssertFailure(Equal(1e-9f, -1e-9f));
    AssertFailure(Equal(1e-2f, -1e-2f));

    // large magnitude numbers
    AssertSuccess(Equal(1e8f, 1e8f + 1.0f));
    AssertFailure(Equal(1e8f, 1e8f + 1e4f));
    AssertSuccess(Equal(1e16, 1e16 + 1.0));
    AssertFailure(Equal(1e16, 1e16 + 1e8));

    // nan and infinity
    AssertSuccess(Equal(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()));
    AssertFailure(Equal(std::numeric_limits<float>::infinity(), 1e30f));
    AssertFailure(Equal(std::nanf(""), std::nanf("")));

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

    std::println("--- --- [nm] assert testing finished successfully");

    std::println("--- --- [nm] test testing started");
    {
        Result res;
        assert(res.Success());
        assert(res.Messages().empty());

        res
        & Equal(1,2, "Cannot be equal")
        & NotEqual(1,1, "Cannot be NOT equal");
        assert(res.Success() == false);
        assert(res.Messages().size() == 2);
    }

    {
        const auto res = Equal(1,2) & NotEqual(1,1);
        assert(res.Success() == false);
        assert(res.Messages().size() == 2);
    }

    {
        auto res = []
        {
            return Equal(1,2)
            & NotEqual(1,1);
        };
        assert(res().Success() == false);
        assert(res().Messages().size() == 2);
    }
    std::println("--- --- [nm] test testing finished successfully");

    /*
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
    */

    std::println("--- [nm] testing finished successfully");
}
