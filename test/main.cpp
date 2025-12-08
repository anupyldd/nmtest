#include <array>
#include <cassert>
#include <print>

#include "test_header.h"

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
    {
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

    {
        using namespace names;

        std::println("--- --- --- should print only 1 'Registry was created'");
        auto reg = Registry();
        auto reg2 = Registry();
        auto reg3 = Registry();

        auto& t1 = Test(suite1, AddLoc("Addition"), {tag1, tag2});
        t1.Setup([]{ std::println("expected setup func 1"); });
        t1.Teardown([]{ std::println("expected teardown func 1"); });
        t1.Func([]
        {
            return Equal(1+1, 2)
            & Equal(2+2, 4);
        });

        Test(suite1, AddLoc("Subtraction"))
        .Setup([]{ std::println("expected setup func 2"); })
        .Teardown([]{ std::println("expected teardown func 2"); })
        .Func([]
        {
            return Equal(2 - 1, 5)
            & Equal(1, 1)
            & NotEqual(2, 2);
        });

        Test(suite1, AddLoc("Multiplication"))
        .Func([]
        {
            auto res = Equal(2 * 2, 5, "custom message");
            res & NotEqual(1, 1);
            return res;
        });

        Suite(suite1)
        .Setup([]{std::println("expected Math setup");})
        .Teardown([]{std::println("expected Math teardown");})
        .Test({
            .name = AddLoc("FromSuite 1"),
            .func = []{ return Equal(0,9); }
        })
        .Test({
            .name = AddLoc("FromSuite 2"),
            .tags = {tag1, tag2},
            .func = []{ return Equal(0,8); },
            .setup = []{ std::println("expected FromSuite 2 setup"); },
            .teardown = []{ std::println("expected FromSuite 2 teardown"); },
        });

        auto ref = Registry();

        Run();

        //Suite("Core",)
    }

    {
        auto cli = CLI();

        const char* argv[] = { "test", "-s", "math,core", "-t", "fast , slow", "-v", "-c", "-l", "-h" };
        auto argc = 9;
        const auto argVec = std::vector<std::string>(argv + 1, argv + argc);

        const auto q = cli.GetParser().GetQuery(argVec);
        assert(q.has_value());

        const auto [suites, tags, flags] = q.value();
        assert(suites.size() == 2);
        assert(tags.size() == 2);
        assert(suites[0] == "math");
        assert(suites[1] == "core");
        assert(tags[0] == "fast");
        assert(tags[1] == "slow");

        assert(flags & cli.help);
        assert(flags & cli.list);
        assert(flags & cli.caseSensitive);
        assert(flags & cli.verbose);
    }

    {
        const char* argv[] = { "test", "-s", "math,core", "-t", "fast , slow", "-v", "-c", "-l", "-h" };
        auto argc = 9;
    }

    std::println("--- --- [nm] test testing finished successfully");

    std::println("--- [nm] testing finished successfully");
}
