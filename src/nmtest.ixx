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
#include <print>
#include <set>

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
        if constexpr (what.empty())
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
            std::println("{} {}:", fail, testName);
            for (const auto& msg : res.Messages())
                std::println("{} {}", indent, msg);
        }
    }

    constexpr auto ReportSummary(
        int total,
        int passed,
        const std::vector<std::string>& failed,
        const std::vector<std::string>& errors) -> void
    {
        std::println("{} Total: {}; Passed: {}; Failed: {}; Errors: {}",
            summary, passed, failed.size(), errors.size());

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
        std::println("{} {}: missing the test function", indent, testName);
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

    class Registry
    {
    public:
        struct TestCase
        {
            TestCase(
                std::string name,
                const std::function<nm::Result()>& func,
                const std::function<void()>& setup = std::function<void()>(),
                const std::function<void()>& teardown = std::function<void()>())
                    : setup(setup), func(func), teardown(teardown), name(std::move(name)) {}

            std::function<void()> setup;
            std::function<void()> teardown;
            std::function<nm::Result()> func;
            std::string name;
        };

        // query to the registry for filtering
        struct Query
        {
            std::vector<std::string> suites;
            std::vector<std::string> tags;
            std::vector<std::string> excTags;   // excluded tags
        };

        // structure containing query error info (e.g. tags/suites that were not found)
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
        auto AddTest(
            const TestCase& test,
            const std::string& suite,
            const std::initializer_list<std::string>& tags) -> void
        {
            allTests.emplace_back(test);
            const auto index = allTests.size() - 1;

            suiteIndex[suite].push_back(index);
            for (const auto& tag : tags)
                tagIndex[tag].push_back(index);
        }

        // get all tests
        [[nodiscard]]
        auto AllTests() const -> const std::vector<TestCase>&
        {
            return allTests;
        }

        // get a list of test indices filtered by the query
        [[nodiscard]]
        auto FilterTests(const Query& query) const
            -> std::expected<std::vector<std::size_t>, QueryError>
        {
            std::vector<std::size_t> tests;
            tests.reserve(allTests.size());
            QueryError error; // created in advance for possible errors

            for (const auto& suite : query.suites)
            {
                std::vector<std::size_t> curSuite;
                try
                {
                    curSuite = suiteIndex.at(suite);
                }
                catch (const std::exception& e)
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
                    catch (const std::exception& e)
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

        // run tests with optional filtering
        auto Run(const int argc, char** argv) -> void
        {
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
        }

    private:
        // parse cli into a query
        [[nodiscard]]
        auto ParseArgs(int argc, char** argv) -> Query
        {
            return Query{};
        }

        // try to run a function
        template<typename T>
        auto TryRunFunc(
            const std::string& testName,
            const FuncType funcType,
            const std::function<T()>& func) -> TestStatus
        {
            const std::string type =
                (funcType == FuncType::Setup) ? "Setup" :
                (funcType == FuncType::Test)  ? "Test"  : "Teardown";

            if (func)
            {
                try
                {
                    if (const std::optional<nm::Result> res = func())
                    {
                        fmt::ReportResult(testName, res.value());

                        if (res->Success())
                            return TestStatus::Pass;
                        return TestStatus::Fail;
                    }
                    else
                    {
                        return TestStatus::None;
                    }
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
            else
            {
                if (funcType == FuncType::Test)
                {
                    fmt::ReportMissingTest(testName);
                    return TestStatus::Error;
                }
                return TestStatus::None;
            }
        }

    private:
        std::unordered_map<std::string, std::vector<std::size_t>> tagIndex;
        std::unordered_map<std::string, std::vector<std::size_t>> suiteIndex;
        std::vector<TestCase> allTests;
    };
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