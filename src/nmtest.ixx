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
#include <set>

export module nm;

// API
export namespace nm
{
    class Result;       // result of an assert or test
}

namespace fmt
{
    auto LogIndent = "          ";

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
        auto Run(const Query& query = Query{})
        {

        }

    private:
        std::unordered_map<std::string, std::vector<std::size_t>> tagIndex;
        std::unordered_map<std::string, std::vector<std::size_t>> suiteIndex;
        std::vector<TestCase> allTests;
    };
}

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

    /*

    class Report
    {
        friend Registry;

    public:
        Report& operator &= (const Result& rhs)
        {
            success = (success && rhs.success);
            if (!rhs.success) messages.push_back(rhs.message);
            return *this;
        }

        Report& operator & (const Result& rhs)
        {
            success = (success && rhs.success);
            if (!rhs.success) messages.push_back(rhs.message);
            return *this;
        }

        // intentionally implicit
        operator bool () const
        {
            return success;
        }

        [[nodiscard]]
        auto Format() const -> std::string
        {
            std::stringstream sstr;
            for (const auto& msg : messages)
                sstr << detail::LogIndent << msg << '\n';
            return sstr.str();
        }

        // get the list of all aggregated messages
        [[nodiscard]]
        auto Messages() const -> const std::vector<std::string>&
        {
            return messages;
        }

        // get the success status
        [[nodiscard]]
        auto Success() const -> bool
        {
            return success;
        }

    private:
        std::vector<std::string> messages;
        bool success = true;
    };

    class Test
    {
        friend Suite;
        friend Registry;

    public:
        Test(std::string name) : name(std::move(name)) {}

        // add test function
        auto Func(std::function<Report()> func) -> Test&
        {
            test = std::move(func);
            return *this;
        }

        // get the test function
        [[nodiscard]]
        auto Func() const -> const std::function<Report()>&
        {
            return test;
        }

        // add setup function (will be called before the test)
        auto Setup(std::function<void()> func) -> Test&
        {
            setup = std::move(func);
            return *this;
        }

        // get the setup function
        [[nodiscard]]
        auto Setup() const -> const std::function<void()>&
        {
            return setup;
        }

        // add teardown function (will be called after the test)
        auto Teardown(std::function<void()> func) -> Test&
        {
            teardown = std::move(func);
            return *this;
        }

        // get the teardown function
        [[nodiscard]]
        auto Teardown() const -> const std::function<void()>&
        {
            return teardown;
        }

        // get the test name
        [[nodiscard]]
        auto Name() const -> const std::string&
        {
            return name;
        }

    private:
        std::function<void()> setup;
        std::function<Report()> test;
        std::function<void()> teardown;
        std::string name;
    };

    class Suite
    {
        friend Test;
        friend Registry;

    public:
        Suite(std::string name) : name(std::move(name)) {}

        // add setup function (will be called before the start of this suite)
        auto Setup(std::function<void()> func) -> Suite&
        {
            setup = std::move(func);
            return *this;
        }

        // get the setup function
        [[nodiscard]]
        auto Setup() const -> const std::function<void()>&
        {
            return setup;
        }

        // add teardown function (will be called after the end of this suite)
        auto Teardown(std::function<void()> func) -> Suite&
        {
            teardown = std::move(func);
            return *this;
        }

        // get the teardown function
        [[nodiscard]]
        auto Teardown() const -> const std::function<void()>&
        {
            return teardown;
        }

        // add tests to the registry
        template<typename T, typename... Ts>
        requires (std::same_as<std::decay_t<T>, Test> &&
                 (std::same_as<std::decay_t<Ts>, std::decay_t<T>> && ...))
        auto Add(T&& first, Ts&&... rest) -> void
        {
            tests.emplace_back(std::forward<T>(first));
            (tests.emplace_back(std::forward<Ts>(rest)), ...);
        }

        // get the list of added tests
        [[nodiscard]]
        auto Tests() const -> const std::vector<Test>&
        {
            return tests;
        }

        // get the suite name
        [[nodiscard]]
        auto Name() const -> const std::string&
        {
            return name;
        }

    private:
        std::function<void()> setup;
        std::function<void()> teardown;

        std::string name;
        std::vector<Test> tests;
    };

    class Registry
    {
    public:
        // add a test suite to the registry
        auto Suite(std::string name) -> nm::Suite&
        {
            return suites.emplace_back(std::move(name));
        }

        // get the list of added suites
        [[nodiscard]]
        auto Suites() const -> const std::vector<nm::Suite>&
        {
            return suites;
        }

        // run all tests in all suites and report the results
        auto Run() -> void
        {
            int total  = 0,
                failed = 0,
                errors = 0;
            std::vector<std::string> fails;

            for (auto& suite : suites)
            {
                if (suite.setup) suite.setup();
                std::println("[  SUITE] {}", suite.name);

                for (auto& test : suite.tests)
                {
                    auto& testName = test.name;

                    if (!test.test)
                    {
                        std::println("[! ERROR] Test '{}': missing the test function", testName);
                        continue;
                    }

                    // setup ----------
                    try
                    {
                        if (test.setup) test.setup();
                    }
                    catch (const std::exception& e)
                    {
                        std::println("[! ERROR] Test '{}': Setup function has thrown an unhandled exception '{}'",
                            testName, e.what());
                        ++errors;
                        continue;
                    }
                    catch (...)
                    {
                        std::println("[! ERROR] Test '{}': Setup function has thrown an unknown unhandled exception",
                            testName);
                        ++errors;
                        continue;
                    }

                    // test ----------
                    try
                    {
                        if (auto rep = test.test())
                        {
                            std::println("[   PASS] {}", testName);
                        }
                        else
                        {
                            std::println("[X  FAIL] {}:\n{}", testName, rep.Format());
                            ++failed;
                            fails.push_back(testName);
                        }
                    }
                    catch (const std::exception& e)
                    {
                        std::println("[! ERROR] Test '{}': Test function has thrown an unhandled exception '{}'",
                            testName, e.what());
                        ++errors;
                        continue;
                    }
                    catch (...)
                    {
                        std::println("[! ERROR] Test '{}': Test function has thrown an unknown unhandled exception",
                            testName);
                        ++errors;
                        continue;
                    }

                    // teardown ----------
                    try
                    {
                        if (test.teardown) test.teardown();
                    }
                    catch (const std::exception& e)
                    {
                        std::println("[! ERROR] Test '{}': Teardown function has thrown an unhandled exception '{}'",
                            testName, e.what());
                        ++errors;
                        continue;
                    }
                    catch (...)
                    {
                        std::println("[! ERROR] Test '{}': Teardown function has thrown an unknown unhandled exception",
                            testName);
                        ++errors;
                        continue;
                    }

                    ++total;
                }


                if (suite.teardown) suite.teardown();
            }

            std::println("[SUMMARY] Total: {}; Succeeded: {}; Failed: {}; Errors: {}",
                total, total - failed, failed, errors);

            if (failed > 0)
            {
                std::println("{}Failed tests:", detail::LogIndent);
                for (const auto& name : fails)
                    std::println("          - {}", name);
            }
        }

    private:
        std::vector<nm::Suite> suites;
    };

    */
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