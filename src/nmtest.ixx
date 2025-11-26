module;

#include <utility>
#include <functional>
#include <string>
#include <algorithm>
#include <exception>
#include <expected>
#include <unordered_map>
#include <format>
#include <source_location>
#include <filesystem>
#include <iostream>
#include <print>
#include <ranges>

export module nm;

export namespace nm
{
    class Result
    {
    public:
        Result(
            const bool success = true,
                  std::string type = std::string(),       // type of assert (e.g. "Equal")
                  std::string message = std::string(),    // custom message provided by the user
            const std::source_location& loc = std::source_location())
            : success(success)
        {
            if (!success)
            {
                const auto filename = std::filesystem::path(loc.file_name()).filename().string();
                if (message.empty())
                    messages.push_back(std::format("{} | {}:{}",
                        std::move(type), filename, loc.line()));
                else
                    messages.push_back(std::format("{} | {}:{} | {}",
                        std::move(type), filename, loc.line(), message));
            }
        }

        // intentionally implicit
        constexpr operator bool() const
        {
            return success;
        }

        constexpr Result& operator & (const Result& rhs)
        {
            success = (success && rhs.success);
            if (!rhs.success)
                for (const auto& msg : rhs.messages)
                    messages.push_back(msg);
            return *this;
        }

        // get the message
        [[nodiscard]]
        constexpr auto Messages() const -> const std::vector<std::string>&
        {
            return messages;
        }

        // get the success status
        [[nodiscard]]
        constexpr auto Success() const -> bool
        {
            return success;
        }

    private:
        std::vector<std::string> messages;
        bool success;
    };
}

namespace impl
{
    enum class FuncType : std::uint8_t
    {
        Setup,
        Test,
        Teardown
    };
}

namespace fmt
{
    const auto indent     = "         "; // "         " log prefix
    const auto suite      = "[  SUITE]"; // "[  SUITE]" log prefix
    const auto pass       = "[   PASS]"; // "[   PASS]" log prefix
    const auto error      = "[! ERROR]"; // "[! ERROR]" log prefix
    const auto fail       = "[X  FAIL]"; // "[X  FAIL]" log prefix
    const auto summary    = "[SUMMARY]"; // "[SUMMARY]" log prefix

    // format assert with actual and expected values
    template<typename T>
    [[nodiscard]]
    constexpr auto ActualExpected(
        const std::string& type,
        const T& actual,
        const T& expected) -> std::string
    {
        return std::format("{}({}:{})", type, actual, expected);
    }

    // print an error like "[! ERROR] Test 1: Setup function has thrown an unhandled exception 'whatever'"
    constexpr auto ReportException(
        const impl::FuncType type,
        const std::string& name,
        const std::string& what = std::string()) -> void
    {
        const auto funcType = (type == impl::FuncType::Setup) ?
            "Setup" : (type == impl::FuncType::Teardown) ?
            "Teardown" :
            "Test";

        if (what.empty())
            std::println("{} {}: {} function has thrown an unhandled exception",
                fmt::error, name, funcType);
        else
            std::println("{} {}: {} function has thrown an unhandled exception '{}'",
                fmt::error, name, funcType, what);
    };

    constexpr auto ReportResult(
        const std::string& testName,
        const nm::Result& res) -> void
    {
        if (res.Success())
        {
            std::println("{} {}", pass, testName);
        }
        else
        {
            std::println("{} {}: ", fail, testName);
            for (const auto& msg : res.Messages())
                std::println("{} - {}", indent, msg);
        }
    }

    auto ReportSummary(
        const int total,
        const int passed,
        const std::vector<std::string>& failed,
        const std::vector<std::string>& errors) -> void
    {
        std::cout <<
            summary << " Total: " <<
            total << "; Passed: " <<
            passed << "; Failed: " <<
            failed.size() << "; Errors: " <<
            errors.size() << '\n';
        //std::println("{} Total: {}; Passed: {}; Failed: {}; Errors: {}",
        //    summary, passed, failed.size(), errors.size());

        if (!failed.empty())
        {
            std::println("{} Failed tests:", indent);
            for (const auto& f : failed)
                std::println("{} - {}", indent, f);
        }
        if (!errors.empty())
        {
            std::println("{} Tests with errors:", indent);
            for (const auto& e : errors)
                std::println("{} - {}", indent, e);
        }
    }

    void ReportMissingTest(const std::string& testName)
    {
        std::println("{} {}: missing the test function", error, testName);
    }
}

namespace types
{
    // integral type, excludes char and bool
    template<typename T>
    concept Integer = std::is_integral_v<T>     &&
        !std::is_same_v<T,          bool    >   &&
        !std::is_same_v<T,          char    >   &&
        !std::is_same_v<T, signed   char    >   &&
        !std::is_same_v<T, unsigned char    >   &&
        !std::is_same_v<T,          char8_t >   &&
        !std::is_same_v<T,          char16_t>   &&
        !std::is_same_v<T,          char32_t>   &&
        !std::is_same_v<T,          wchar_t>;

    template<typename T>
    concept FloatingPoint = std::is_floating_point_v<T>;

    template<typename T>
    concept Number = Integer<T> || FloatingPoint<T>;

}

namespace impl
{
    constinit auto FloatEpsilon  = 1.192092896e-07F;
    constinit auto FloatMin      = 1.175494351e-38F;
    constinit auto DoubleEpsilon = 2.2204460492503131e-016;
    constinit auto DoubleMin     = 2.2250738585072014e-308;

    // compares floating point numbers for equality
    template<types::FloatingPoint T>
    [[nodiscard]]
    constexpr auto AlmostEqual(T a, T b) noexcept -> bool
    {
        // different epsilon for float and double
        constexpr bool isFloat = std::is_same_v<T, float_t>;
        const auto epsilon = isFloat ? 128 * FloatEpsilon : 128 * DoubleEpsilon;
        const auto absTh  = isFloat ? FloatMin : DoubleMin;

        if (a == b) return true;

        const auto diff = std::abs(a - b);
        const auto norm = std::min({std::abs(a + b), std::numeric_limits<T>::max()});
        return diff < std::max(absTh, epsilon * norm);
    }

    // compares any numbers including float and double
    template<typename T>
    [[nodiscard]]
    constexpr auto NumericEqual(T a,T b) -> bool
    {
        if constexpr (types::FloatingPoint<T>)
            return AlmostEqual<T>(a, b);
        return (a == b);
    }

    template<typename F, typename Tuple = std::tuple<>>
    [[nodiscard]]
    constexpr auto Throws(F&& func, Tuple&& argsTuple = {}) -> bool
    {
        try
        {
            std::apply(std::forward<F>(func), std::forward<Tuple>(argsTuple));
            return false;
        }
        catch (...)
        {
            return true;
        }
    }

    template<typename T>
    [[nodiscard]]
    constexpr auto Null(T val) -> bool
    {
        return static_cast<bool>(!val);
    }

    template<typename T>
    [[nodiscard]]
    constexpr auto True(T val) -> bool
    {
        return val;
    }

    // REGISTRY ---------------------------------------------------------------

    class Registry
    {
    public:
        // setup, teardown, tags
        class TestBase
        {
        public:
            TestBase(
                const std::vector<std::string>& tags = std::vector<std::string>(),
                      std::function<void()>  setup = std::function<void()>(),
                      std::function<void()>  teardown = std::function<void()>())
                    : setup(std::move(setup)), teardown(std::move(teardown)), tags(tags) {}

            virtual ~TestBase() = default;

            // get the setup function
            [[nodiscard]]
            virtual auto Setup() const noexcept -> const std::function<void()>&
            {
                return setup;
            }

            // get the teardown function
            [[nodiscard]]
            virtual auto Teardown() const noexcept -> const std::function<void()>&
            {
                return teardown;
            }

            // get tags
            [[nodiscard]]
            virtual auto Tags() const noexcept -> const std::vector<std::string>&
            {
                return tags;
            }

        protected:
            std::function<void()> setup;
            std::function<void()> teardown;
            std::vector<std::string> tags;
        };

        class TestCase final : public TestBase
        {
        public:
            TestCase(
                const std::vector<std::string>& tags = std::vector<std::string>(),
                      std::function<nm::Result()> func = std::function<nm::Result()>(),
                      std::function<void()> setup = std::function<void()>(),
                      std::function<void()> teardown = std::function<void()>())
                    : TestBase(tags, std::move(setup), std::move(teardown)), func(std::move(func)) {}

            ~TestCase() override = default;

            // add the test function
            auto Func(const std::function<nm::Result()>& fn) -> TestCase&
            {
                if (fn) func = fn;
                return *this;
            }

            // get the test function
            [[nodiscard]]
            auto Func() const -> const std::function<nm::Result()>&
            {
                return func;
            }

            // add the setup function
            auto Setup(const std::function<void()>& fn) -> TestCase&
            {
                if (fn) setup = fn;
                return *this;
            }

            // add the setup function
            auto Teardown(const std::function<void()>& fn) -> TestCase&
            {
                if (fn) teardown = fn;
                return *this;
            }

            // add tags
            auto Tags(const std::initializer_list<std::string>& tagList) -> TestCase&
            {
                tags.reserve(tags.size() + tagList.size());
                for (const auto& tag : tagList)
                    tags.push_back(tag);
                return *this;
            }

        private:
            std::function<nm::Result()> func;
        };

        class TestSuite final : public TestBase
        {
        public:
            TestSuite(
                const std::vector<std::string>& tags = std::vector<std::string>(),
                      std::function<void()> setup = std::function<void()>(),
                      std::function<void()> teardown = std::function<void()>())
                    : TestBase(tags, std::move(setup), std::move(teardown)) {}

            ~TestSuite() override = default;

            // add the setup function
            auto Setup(const std::function<void()>& fn) -> TestSuite&
            {
                if (fn) setup = fn;
                return *this;
            }

            // add the setup function
            auto Teardown(const std::function<void()>& fn) -> TestSuite&
            {
                if (fn) teardown = fn;
                return *this;
            }

            // add a test
            auto Add(
                const std::string& name,
                const TestCase& test) -> TestCase&
            {
                tests.emplace_back(name, test);
                return tests.back().second;
            }

            // add an empty test (without test fn)
            auto Add(const std::string& name) -> TestCase&
            {
                tests.emplace_back(name, TestCase{});
                return tests.back().second;
            }

            // add tags
            auto Tags(const std::initializer_list<std::string>& tagList) -> TestSuite&
            {
                tags.reserve(tags.size() + tagList.size());
                for (const auto& tag : tagList)
                    tags.push_back(tag);
                return *this;
            }

            // get list of test indices
            [[nodiscard]]
            auto Tests() const -> const std::vector<std::pair<std::string, TestCase>>&
            {
                return tests;
            }

            // get list of test indices
            [[nodiscard]]
            auto Tests() -> std::vector<std::pair<std::string, TestCase>>&
            {
                return tests;
            }

        private:
            std::vector<std::pair<std::string, TestCase>> tests;
        };

        // query to the registry for filtering
        struct Query
        {
            std::vector<std::string> suites;
            std::vector<std::string> tags;
        };

        // structure containing query error info (tags/suites that were not found)
        struct QueryError
        {
            std::vector<std::string> tags;
            std::vector<std::string> suites;
        };

        enum class TestStatus : std::uint8_t
        {
            Pass,
            Fail,
            Error
        };

        struct Summary
        {
            int  total  = 0,
                 passed = 0;
            std::vector<std::string> failed,
                                     errors;
        };

    public:
        // register a suite
        auto AddSuite(
            const std::string& name,
            const TestSuite& suite) -> TestSuite&
        {
            suites[name] = suite;
            return suites[name];
        }

        // register a test
        auto AddTest(
            const std::string& suite,
            const std::string& name,
            const TestCase& test) -> TestCase&
        {
            return suites[suite].Add(name, test);
        }

        // get all tests
        [[nodiscard]]
        auto AllTests() const -> const std::unordered_map<std::string, TestSuite>&
        {
            return suites;
        }

        // run tests with optional filtering
        auto Run(const auto& query) -> void
        {
            Summary summary;

            // iterate over suites that are in query.suites
            for (const auto& [suiteName, suite] : suites
                | std::views::filter([&](const auto& pair)
                {
                    if (query.suites.empty()) return true;
                    return std::ranges::contains(query.suites, pair.first);
                }))
            {
                TryInvoke(suiteName, &suite, FuncType::Setup, summary);

                for (const auto& [testName, test] : suite.Tests()
                    | std::views::filter([&](const auto& pair)
                    {
                        if (query.tags.empty()) return true;
                        for (const auto& tag : pair.second.TestBase::Tags())
                            if (std::ranges::contains(query.tags, tag)) return true;
                        return false;
                    }))
                {
                    TryInvoke(testName, &test, FuncType::Setup,    summary);
                    TryInvoke(testName, &test, FuncType::Test,     summary);
                    TryInvoke(testName, &test, FuncType::Teardown, summary);
                }

                TryInvoke(suiteName, &suite, FuncType::Teardown, summary);
            }

            fmt::ReportSummary(summary.total, summary.passed, summary.failed, summary.errors);
        }

        // parse cli into a query
        [[nodiscard]]
        auto ParseArgs(int argc, char** argv) -> Query
        {
            return Query{};
        }

        // get specific suite. create if not found
        [[nodiscard]]
        auto GetSuite(const std::string& name) -> TestSuite&
        {
            return suites[name];
        }

        // get specific test. create if not found
        [[nodiscard]]
        auto GetTest(
            const std::string& suiteName,
            const std::string& testName) -> TestCase&
        {
            auto& suite = GetSuite(suiteName);

            for (auto& [name, test] : suite.Tests())
            {
                if (name == testName) return test;
            }

            auto& newTest = suite.Add(testName, TestCase{});
            return newTest;
        }

        // set last test
        auto LastTest(TestCase* test) -> void
        {
            lastTest = test;
        }
        // get last test
        [[nodiscard]]
        auto LastTest() const -> TestCase*
        {
            return lastTest;
        }

        // set last suite
        auto LastSuite(TestSuite* suite) -> void
        {
            lastSuite = suite;
        }
        // get last suite
        [[nodiscard]]
        auto LastSuite() const -> TestSuite*
        {
            return lastSuite;
        }

    private:
        // try to run a function from a suite
        static auto TryInvoke(
            const std::string& name,
            const TestBase*    testOrSuite,
            const FuncType     funcType,
                  Summary&     summary) -> std::optional<TestStatus>
        {
            try
            {
                if (const auto* suite = dynamic_cast<const TestSuite*>(testOrSuite))
                {
                    switch (funcType)
                    {
                        case FuncType::Setup:
                        {
                            if (suite->TestBase::Setup())
                                suite->TestBase::Setup()();
                        }
                        break;

                        case FuncType::Teardown:
                        {
                            if (suite->TestBase::Teardown())
                                suite->TestBase::Teardown()();
                        }
                        break;

                        default: break;
                    }

                    return std::nullopt;
                }

                if (const auto* test = dynamic_cast<const TestCase*>(testOrSuite))
                {
                    switch (funcType)
                    {
                        case FuncType::Setup:
                        {
                            if (test->TestBase::Setup())
                                test->TestBase::Setup()();
                            return std::nullopt;
                        }

                        case FuncType::Teardown:
                        {
                            if (test->TestBase::Teardown())
                                test->TestBase::Teardown()();
                            return std::nullopt;
                        }

                        case FuncType::Test:
                        {
                            ++summary.total;

                            if (!test->Func())
                            {
                                fmt::ReportMissingTest(name);
                                return TestStatus::Error;
                            }

                            const auto res = test->Func()();
                            fmt::ReportResult(name, res);
                            if (res)
                            {
                                ++summary.passed;
                                return TestStatus::Pass;
                            }
                            summary.failed.emplace_back(name);
                            return TestStatus::Fail;
                        }
                    }
                }

                return TestStatus::Error;
            }
            catch (const std::exception& e)
            {
                fmt::ReportException(funcType, name, e.what());
                summary.errors.emplace_back(name);
                return TestStatus::Error;
            }
            catch (...)
            {
                fmt::ReportException(funcType, name);
                summary.errors.emplace_back(name);
                return TestStatus::Error;
            }


        }

    private:
        std::unordered_map<std::string, TestSuite> suites;
        TestSuite* lastSuite = nullptr;
        TestCase*  lastTest  = nullptr;
    };

    // get static Registry instance
    auto GetRegistry() -> Registry&
    {
        static Registry reg;
        return reg;
    }

    // ------------------------------------------------------------------------

    template<std::size_t N>
    struct StructuralString
    {
        constexpr StructuralString(const char (&str)[N])
        {
            std::copy_n(str, N, string);
        }

        constexpr operator std::string_view () const { return string; }
        constexpr operator std::string () const { return string; }

        char string[N] {};
    };

    struct SuiteName
    {
        SuiteName(const std::string& name)
        {
            using namespace impl;
            if (!name.empty()) GetRegistry().LastSuite(&(GetRegistry().GetSuite(name)));
            std::println("!!!!! created suite name");
        }
        SuiteName(const char* name) : SuiteName(std::string(name)) {}
    };
    struct TestName
    {
        TestName(const std::string& name)
        {
            using namespace impl;
            if (!name.empty()) GetRegistry().LastTest(&(GetRegistry().LastSuite()->Add(name)));
            std::println("!!!!! created test name");
        }
        TestName(const char* name) : TestName(std::string(name)) {}
    };
    struct TestFunc
    {
        template<class F, class = std::enable_if_t<std::is_invocable_r_v<nm::Result, F>>>
        TestFunc(F&& f)
        {
            using namespace impl;
            const std::function<nm::Result()> fn = std::forward<F>(f);
            if (fn) GetRegistry().LastTest()->Func(fn);
            std::println("!!!!! created test func");
        }
    };
    struct SetupFunc
    {
        template<class F, class = std::enable_if_t<std::is_invocable_r_v<void, F>>>
        SetupFunc(F&& f)
        {
            using namespace impl;
            const std::function<void()> fn = std::forward<F>(f);
            GetRegistry().LastTest()->Setup(fn);
            std::println("!!!!! created test func");
        }
    };
}

export namespace nm
{
    // structure for registering a test
    struct TestS
    {
        impl::SuiteName suite;
        impl::TestName test;
        impl::TestFunc func;
    };

    // template structure for registering a test
    template<impl::StructuralString suite,
             impl::StructuralString name,
             nm::Result (* func) ()>
    struct TestT
    {
        TestT()
        {
            using namespace impl;
            GetRegistry().LastTest(&(GetRegistry().AddTest(suite, name, Registry::TestCase{})));
            GetRegistry().LastTest()->Func(func);
        }

        static const TestT registerer;
    };
    template <impl::StructuralString suite,
              impl::StructuralString name,
              nm::Result (* func) ()>
    const TestT<suite, name, func> TestT<suite, name, func>::registerer;

    // ------------------------------------------------------------------------

    // register a test function
    auto Test(
        const std::string& suite,
        const std::string& name,
        const std::initializer_list<std::string>& tags =
              std::initializer_list<std::string>()) -> impl::Registry::TestCase&
    {
        return impl::GetRegistry().AddTest(suite, name, impl::Registry::TestCase(tags));
    }

    // add a suite with any number of tests
    auto Suite(const std::string& name, const std::initializer_list<std::string>& tags =
              std::initializer_list<std::string>()) -> void
    {
        using namespace impl;
        GetRegistry().AddSuite(name, impl::Registry::TestSuite(tags));
    }

    // get the registry
    auto Registry() -> impl::Registry&
    {
        return impl::GetRegistry();
    }

    // run all tests with optional filtering
    auto Run(const int argc = 1, char** argv = nullptr) -> void
    {
        const auto query = impl::GetRegistry().ParseArgs(argc, argv);
        impl::GetRegistry().Run(query);
    }
}

export namespace nm
{
    // succeeds if the actual value is equal to the expected value
    // works for floating point numbers as well
    template<typename T>
    [[nodiscard]]
    constexpr auto Equal(
        const T& actual,
        const T& expected,
        const std::string& message = std::string(),
        const std::source_location loc = std::source_location::current()) -> Result
    {
        const auto res = (types::Number<T>) ? impl::NumericEqual(actual, expected) : (actual == expected);
        return res ? Result(true) : Result(false, fmt::ActualExpected("Equal", actual, expected), message, loc);
    }

    // succeeds if actual value is NOT equal to the expected value
    // works for floating point numbers as well
    template<typename T>
    [[nodiscard]]
    constexpr auto NotEqual(
        const T& actual,
        const T& expected,
        const std::string& message = std::string(),
        const std::source_location loc = std::source_location::current()) -> Result
    {
        const auto res = (types::Number<T>) ? impl::NumericEqual(actual, expected) : (actual == expected);
        return res ? Result(false, fmt::ActualExpected("NotEqual", actual, expected), message, loc) : Result(true);
    }

    // succeeds if passed F (function, functor, lambda) throws any exception
    template<typename F, typename Tuple = std::tuple<>>
    [[nodiscard]]
    constexpr auto Throws(
              F&& func,
              Tuple&& argsTuple = {},
        const std::string& message = std::string(),
        const std::source_location loc = std::source_location::current()) -> Result
    {
        return impl::Throws(std::forward<F>(func), std::forward<Tuple>(argsTuple)) ?
            Result(true) : Result(false, "Throws", message, loc);
    }

    // succeeds if passed F (function / functor / lambda / ...) does not throw any exception
    template<typename F, typename Tuple = std::tuple<>>
    [[nodiscard]]
    constexpr auto DoesNotThrow(
              F&& func,
              Tuple&& argsTuple = {},
        const std::string& message = std::string(),
        const std::source_location loc = std::source_location::current()) -> Result
    {
        return impl::Throws(std::forward<F>(func), std::forward<Tuple>(argsTuple)) ?
            Result(false, "DoesNotThrow", message, loc) : Result(true);
    }

    // succeeds if val is null
    template<typename T>
    [[nodiscard]]
    constexpr auto Null(
              T val,
        const std::string& message = std::string(),
        const std::source_location loc = std::source_location::current()) -> Result
    {
        return impl::Null(val) ? Result(true) : Result(false, "Null", message, loc);
    }

    // succeeds if val is NOT null
    template<typename T>
    [[nodiscard]]
    constexpr auto NotNull(
              T val,
        const std::string& message = std::string(),
        const std::source_location loc = std::source_location::current()) -> Result
    {
        return impl::Null(val) ? Result(false, "NotNull", message, loc) : Result(true);
    }

    // succeeds if val is true
    template<typename T>
    [[nodiscard]]
    constexpr auto True(
              T val,
        const std::string& message = std::string(),
        const std::source_location loc = std::source_location::current()) -> Result
    {
        return impl::True(val) ? Result(true) : Result(false, "True", message, loc);
    }

    // succeeds if val is NOT true
    template<typename T>
    [[nodiscard]]
    constexpr auto False(
              T val,
        const std::string& message = std::string(),
        const std::source_location loc = std::source_location::current()) -> Result
    {
        return impl::True(val) ? Result(false, "False", message, loc) : Result(true);
    }
}