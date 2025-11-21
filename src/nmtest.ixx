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
#include <numeric>
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
        return std::format("{}({} : {})", type, actual, expected);
    }

    // print an error like "[! ERROR] Test 1: Setup function has thrown an unhandled exception 'whatever'"
    constexpr auto ReportException(
        const std::string& funcType,
        const std::string& testName,
        const std::string& what = std::string()) -> void
    {
        if (what.empty())
            std::println("{} {}: {} function has thrown an unhandled exception",
                fmt::error, testName, funcType);
        else
            std::println("{} {}: {} function has thrown an unhandled exception '{}'",
                fmt::error, testName, funcType, what);
    };

    constexpr auto ReportResult(const std::string& testName, const nm::Result& res) -> void
    {
        if (res.Success())
        {
            std::println("{} {}", pass, testName);
        }
        else
        {
            std::println("{} {}: ", fail, testName);
            for (const auto& msg : res.Messages())
                std::println("{} {}", indent, msg);
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

    // compares floating point numbers for equality
    template<types::FloatingPoint T>
    [[nodiscard]]
    constexpr auto AlmostEqual(T a, T b) noexcept -> bool
    {
        // different epsilon for float and double
        constexpr bool isFloat = std::is_same_v<T, float_t>;
        const auto epsilon = isFloat ? 128 * FLT_EPSILON : 128 * DBL_EPSILON;
        const auto absTh  = isFloat ? FLT_MIN : DBL_MIN;

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

    using SetupFunc = std::function<void()>;
    using TeardownFunc = std::function<void()>;
    using TestFunc = std::function<nm::Result()>;

    template<typename K, typename V>
    using VectorPair = std::vector<std::pair<K, V>>;

    class Registry
    {
    public:
        // setup, teardown, tags
        class TestBase
        {
        public:
            TestBase(
                const std::initializer_list<std::string>& tags = std::initializer_list<std::string>(),
                SetupFunc  setup = SetupFunc(), TeardownFunc  teardown = TeardownFunc())
                    : setup(std::move(setup)), teardown(std::move(teardown)), tags(tags) {}

            // get the setup function
            [[nodiscard]]
            auto Setup() const noexcept -> const SetupFunc&
            {
                return setup;
            }

            // get the teardown function
            [[nodiscard]]
            auto Teardown() const noexcept -> const TeardownFunc&
            {
                return teardown;
            }

            // get tags
            [[nodiscard]]
            auto Tags() const noexcept -> const std::vector<std::string>&
            {
                return tags;
            }

        protected:
            SetupFunc setup;
            TeardownFunc teardown;
            std::vector<std::string> tags;
        };

        class TestCase final : public TestBase
        {
            friend Registry;

        public:
            TestCase(
                const std::initializer_list<std::string> tags = std::initializer_list<std::string>(),
                SetupFunc setup = SetupFunc(), TeardownFunc teardown = TeardownFunc(),
                TestFunc func = TestFunc())
                    : TestBase(tags, std::move(setup), std::move(teardown)), func(std::move(func)) {}

            // add the test function
            auto Func(const TestFunc& fn) -> TestCase&
            {
                if (fn) func = fn;
                return *this;
            }

            // get the test function
            [[nodiscard]]
            auto Func() const -> const TestFunc&
            {
                return func;
            }

            // add the setup function
            auto Setup(const SetupFunc& fn) -> TestCase&
            {
                if (fn) setup = fn;
                return *this;
            }

            // add the setup function
            auto Teardown(const TeardownFunc& fn) -> TestCase&
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
            TestFunc func;
        };

        class TestSuite final : public TestBase
        {
        public:
            TestSuite(
                const std::initializer_list<std::string> tags = std::initializer_list<std::string>(),
                SetupFunc setup = SetupFunc(), TeardownFunc teardown = TeardownFunc())
                    : TestBase(tags, std::move(setup), std::move(teardown)) {}

            // add the setup function
            auto Setup(const SetupFunc& fn) -> TestSuite&
            {
                if (fn) setup = fn;
                return *this;
            }

            // add the setup function
            auto Teardown(const TeardownFunc& fn) -> TestSuite&
            {
                if (fn) teardown = fn;
                return *this;
            }

            // add a test
            auto Add(const std::string& name, const TestCase& test) -> TestCase&
            {
                tests.emplace_back(name, test);
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
            auto Tests() const -> const VectorPair<std::string, TestCase>&
            {
                return tests;
            }

        private:
            VectorPair<std::string, TestCase> tests;
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

        enum class FuncType : std::uint8_t
        {
            Setup,
            Test,
            Teardown
        };

        enum class TestStatus : std::uint8_t
        {
            Pass,
            Fail,
            Error,
            None    // status returned by setup and teardown functions
        };

    public:
        Registry() { std::println("Registry was created"); }

        // register a suite
        auto AddSuite(const std::string& name, const TestSuite& suite) -> TestSuite&
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

/*
        // get a list of test indices filtered by the query
        [[nodiscard]]
        auto FilterTests(const Query& query) const
            -> std::expected<std::vector<std::size_t>, QueryError>
        {
            std::vector<std::size_t> tests;
            tests.reserve(allTests.size());
            QueryError error; // created in advance for possible errors

            if (query.tags.empty() && query.suites.empty() &&
                query.excTags.empty() && query.excSuites.empty())
            {
                tests.resize(allTests.size());
                std::iota(tests.begin(), tests.end(), 0);
                return tests;
            }

            for (const auto& suite : query.suites)
            {
                std::vector<std::size_t> curSuite;
                try
                {
                    curSuite = suiteIndex.at(suite);
                }
                catch (const std::exception&)
                {
                    error.suites.push_back(suite);
                    continue;
                }

                for (const auto& tag : query.tags)
                {
                    std::vector<std::size_t> curTag;
                    try
                    {
                        curTag = tagIndex.at(tag);
                    }
                    catch (const std::exception&)
                    {
                        error.tags.push_back(tag);
                        continue;
                    }

                    std::vector<std::size_t> intersect;
                    std::ranges::set_intersection(curSuite, curTag, std::back_inserter(intersect));
                    tests.insert_range(tests.end(), intersect);
                }
            }

            if (error.suites.empty() && error.tags.empty())
                return tests;
            return std::unexpected(error);
        }
*/

        // run tests with optional filtering
        auto Run(const auto& query) -> void
        {
            // iterate over suites that are in query.suites
            for (const auto& [suiteName, suite] : suites
                | std::views::filter([&](const std::string_view sv)
                    {return std::ranges::contains(query.suites, suiteName);}))
            {

            }

            /*
            const auto query = ParseArgs(argc, argv);
            if (const auto res = FilterTests(query))
            {
                const auto& tests = res.value();
                auto total  = 0,
                     passed = 0;

                auto failed = std::vector<std::string>();
                auto errors = std::vector<std::string>();

                for (const auto testIndex : tests)
                {
                    const auto& test = allTests.at(testIndex);

                    TryRunFunc(test.name, FuncType::Setup, test.setup);

                    switch (TryRunFunc(test.name, FuncType::Test, test.func))
                    {
                        case TestStatus::Pass:  ++passed; break;
                        case TestStatus::Fail:  failed.push_back(test.name); break;
                        case TestStatus::Error: errors.push_back(test.name);  break;
                        default: ++total;
                    }

                    TryRunFunc(test.name, FuncType::Teardown, test.teardown);
                }

                fmt::ReportSummary(total, passed, failed, errors);
            }
            else
            {
                std::println("{} Errors detected in the query", fmt::error);

                if (const auto& suites = res.error().suites; suites.empty())
                {
                    std::println("{} Missing suites:", fmt::indent);
                    for (const auto& suite : suites)
                        std::println("{}- {}", fmt::indent, suite);
                }

                if (const auto& tags = res.error().suites; tags.empty())
                {
                    std::println("{} Missing suites:", fmt::indent);
                    for (const auto& tag : tags)
                        std::println("{}- {}", fmt::indent, tag);
                }
            }
            */
        }

        // parse cli into a query
        [[nodiscard]]
        auto ParseArgs(int argc, char** argv) -> Query
        {
            return Query{};
        }

    private:
        // try to run a function from TestCase
        template<typename T>
        requires (std::same_as<T, void> || std::same_as<std::decay_t<T>, nm::Result>)
        auto TryRunTestFunc(
            const std::string& testName,
            const FuncType funcType,
            const std::function<T()>& func) -> TestStatus
        {
            if (!func)
            {
                if (funcType == FuncType::Test)
                {
                    fmt::ReportMissingTest(testName);
                    return TestStatus::Error;
                }
                return TestStatus::None;
            }

            const std::string type =
                funcType == FuncType::Setup ? "Setup" :
                funcType == FuncType::Test  ? "Test"  : "Teardown";

            try
            {
                if constexpr (std::same_as<std::decay_t<T>, nm::Result>)
                {
                    const auto res = func();
                    fmt::ReportResult(testName, res);

                    if (res.Success())
                        return TestStatus::Pass;
                    return TestStatus::Fail;
                }

                func();
                return TestStatus::None;
            }
            catch (const std::exception& e)
            {
                fmt::ReportException(type, testName, e.what());
                return TestStatus::Error;
            }
            catch (...)
            {
                fmt::ReportException(type, testName);
                return TestStatus::Error;
            }
        }

        // try to run a function from a suite
        template<typename T>
        requires (std::same_as<std::decay_t<T>, TestCase> ||
                  std::same_as<std::decay_t<T>, TestSuite>)
        auto TryRunFunc(const T& object, const FuncType funcType) -> std::optional<TestStatus>
        {
            if constexpr (std::is_same_v<std::decay_t<T>, TestCase>)
            {
                switch (funcType)
                {
                    case FuncType::Setup:
                    {
                        if (object.)
                    }
                    break;

                    case FuncType::Teardown:
                    {

                    }
                    break;

                    case FuncType::Test:
                    {

                    }
                    break;
                }
            }
            else
            {

            }
        }

    private:
        std::unordered_map<std::string, TestSuite> suites;
    };

    // get static Registry instance
    auto GetRegistry() -> Registry&
    {
        static Registry reg;
        return reg;
    }
}

export namespace nm
{
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