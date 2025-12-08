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
#include <variant>

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

    struct TestData
    {
        std::string              name;     // test name
        std::vector<std::string> tags;     // tags used for test filtering
        std::function<Result()>  func;     // test function
        std::function<void()>    setup;    // function that runs before the test function
        std::function<void()>    teardown; // function that runs after the test function
    };

    struct SuiteData
    {
        std::string              name;     // suite name
        std::vector<std::string> tags;     // tags used for filtering
        std::function<void()>    setup;    // function that runs before the tests
        std::function<void()>    teardown; // function that runs after the tests
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
    const auto warning    = "[?  WARN]"; // "[?  WARN]" log prefix
    const auto fail       = "[X  FAIL]"; // "[X  FAIL]" log prefix
    const auto summary    = "[SUMMARY]"; // "[SUMMARY]" log prefix

    auto ToLower(std::string str) -> std::string
    {
        std::ranges::transform(str, str.begin(),
            [](const unsigned char c){ return std::tolower(c); });
        return str;
    }

    inline auto LeftTrim(std::string& str) -> void
    {
        str.erase(str.begin(), std::ranges::find_if(str,
            [](const unsigned char ch) { return !std::isspace(ch); }));
    }

    inline auto RightTrim(std::string& str) -> void
    {
        str.erase(std::find_if(str.rbegin(), str.rend(),
            [](const unsigned char ch) { return !std::isspace(ch); }).base(), str.end());
    }

    inline auto Trim(std::string& str) -> void
    {
        LeftTrim(str);
        RightTrim(str);
    }

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

    // print an error like "[! ERROR] some error text (hint)"
    constexpr auto ReportError(
        const std::string& errorText,
        const std::string& hint = {}) -> void
    {
        if constexpr (hint.empty())
            std::println("{} {}", error, errorText);
        else
            std::println("{} {} ({})", error, errorText, hint);
    }

    // TODO: change parameter to option
    // print an error like "[! ERROR] Unknown option 'some param'"
    constexpr auto ReportUnknownParameter(
        const std::string& errorText,
        const std::string& hint = "Use '-h/--help' to see the list of available options") -> void
    {
        ReportError(std::format("Unknown option '{}'", errorText), hint);
    }

    // print an error like "[! ERROR] some error text (hint)"
    constexpr auto ReportWarning(
        const std::string& warningText,
        const std::string& hint = {}) -> void
    {
        if constexpr (hint.empty())
            std::println("{} {}", warning, warningText);
        else
            std::println("{} {} ({})", warning, warningText, hint);
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
    constinit auto floatEpsilon  = 1.192092896e-07F;
    constinit auto floatMin      = 1.175494351e-38F;
    constinit auto doubleEpsilon = 2.2204460492503131e-016;
    constinit auto doubleMin     = 2.2250738585072014e-308;

    // compares floating point numbers for equality
    template<types::FloatingPoint T>
    [[nodiscard]]
    constexpr auto AlmostEqual(T a, T b) noexcept -> bool
    {
        // different epsilon for float and double
        constexpr bool isFloat = std::is_same_v<T, float_t>;
        const auto epsilon = isFloat ? 128 * floatEpsilon : 128 * doubleEpsilon;
        const auto absTh  = isFloat ? floatMin : doubleMin;

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

    // CLI --------------------------------------------------------------------

    class CLI
    {
    public:
        auto helpText =
            "Usage:                                                                     \n"
            "  test [options]                                                           \n"
            "                                                                           \n"
            "Options:                                                                   \n"
            "  -s, --suite <names>         Comma-separated list of suite names to run.  \n"
            "  -t, --tag <names>           Comma-separated list of tag names to run.    \n"
            "  -l, --list                  List matching tests without running them.    \n"
            "  -c, --case_sensitive        Make name matching case-sensitive.           \n"
            "  -h, --help                  Show this help message.                      \n"
            "                                                                           \n"
            "Example:                                                                   \n"
            "  test -s \"core,math\" -t \"fast\"                                        \n"
            "    Run all tests from suites \"core\" or \"math\" that have tag \"fast\". \n"
            "                                                                           \n"
            "Notes:                                                                     \n"
            "  - Multiple suites or tags are treated as OR:                             \n"
            "        -s \"core,math\"     runs tests in either suite.                   \n"
            "        -t \"fast,slow\"     runs tests tagged fast OR slow.               \n"
            "  - Names are case-insensitive unless --case_sensitive is used.            \n";

        struct Query
        {
            std::vector<std::string> suites;
            std::vector<std::string> tags;
        };

        class Parser
        {
        public:
            struct Command
            {
                static constexpr std::uint8_t list           = 1 << 0; // -l / --list
                static constexpr std::uint8_t verbose        = 1 << 1; // -v / --verbose
                static constexpr std::uint8_t caseSensitive  = 1 << 2; // -c / --case_sensitive
                static constexpr std::uint8_t help           = 1 << 3; // -h / --help
                static constexpr std::uint8_t had_error      = 1 << 4; // set if command had any errors

                static constexpr auto suiteTextFull         = "suite";
                static constexpr auto tagTextFull           = "tag";
                static constexpr auto listTextFull          = "list";
                static constexpr auto verboseTextFull       = "verbose";
                static constexpr auto caseSensitiveTextFull = "case_sensitive";
                static constexpr auto helpTextFull          = "help";

                static constexpr auto suiteTextShort         = "s";
                static constexpr auto tagTextShort           = "t";
                static constexpr auto listTextShort          = "l";
                static constexpr auto verboseTextShort       = "v";
                static constexpr auto caseSensitiveTextShort = "c";
                static constexpr auto helpTextShort          = "h";

                std::string  tagArgs,
                             suiteArgs;
                std::uint8_t flags {};

                auto SetFlag(const std::string& arg) -> void
                {
                    const auto argNorm = fmt::ToLower(arg);

                    if (argNorm == listTextShort || argNorm == listTextFull)
                        flags |= list;
                    else if (argNorm == verboseTextShort || argNorm == verboseTextFull)
                        flags |= verbose;
                    else if (argNorm == caseSensitiveTextShort || argNorm == caseSensitiveTextFull)
                        flags |= caseSensitive;
                    else if (argNorm == helpTextShort || argNorm == helpTextFull)
                        flags |= help;
                    else
                    {
                        fmt::ReportUnknownParameter(argNorm);
                        Error();
                    }
                }

                // set the 'had_error' flag
                auto Error() -> void
                {
                    flags |= had_error;
                }
            };

        public:
            static auto GetQuery(const int argc, char** argv) -> std::optional<Query>
            {
                const auto cmd = ProcessCommand(argc, argv);
                if (cmd.flags & Command::had_error) return std::nullopt;

                auto query = Query{};
                query.suites = ParseNames(cmd.suiteArgs);
                query.tags   = ParseNames(cmd.tagArgs);

                return query;
            }

        private:
            static auto ProcessCommand(const int argc, char** argv) -> Command
            {
                const auto argVec = std::vector<std::string>(argv + 1, argv + argc);
                auto cmd = Command{};

                enum class Awaiting { SuiteFilter, TagFilter, Any };
                auto await = Awaiting::Any;

                for (const auto& arg : argVec)
                {
                    if (arg.starts_with("--"))
                    {
                        if (arg.contains('='))
                        {
                            const auto equal = arg.find_first_of('=');
                            const auto param = arg.substr(2, equal - 2);
                            const auto value = arg.substr(equal + 1);

                            if (param == Command::suiteTextFull) cmd.suiteArgs  = value;
                            else if (param == Command::tagTextFull) cmd.tagArgs = value;
                            else
                            {
                                fmt::ReportUnknownParameter(param);
                                cmd.Error();
                            }
                            await = Awaiting::Any;
                        }
                        else
                        {
                            cmd.SetFlag(arg.substr(2));
                        }
                    }
                    else if (arg.starts_with('-'))
                    {
                        const auto param = arg.substr(1);

                        if (param == "s") await = Awaiting::SuiteFilter;
                        else if (param == "t") await = Awaiting::TagFilter;
                        else cmd.SetFlag(param);
                    }
                    else
                    {
                        switch (await)
                        {
                            case Awaiting::SuiteFilter:
                            {
                                if (!cmd.suiteArgs.empty())
                                    fmt::ReportWarning("Overriding suite filter argument with new values",
                                        std::format("was: ", cmd.suiteArgs));
                                cmd.suiteArgs = arg;
                            }
                            break;

                            case Awaiting::TagFilter:
                            {
                                if (!cmd.tagArgs.empty())
                                    fmt::ReportWarning("Overriding tag filter argument with new values",
                                        std::format("was: ", cmd.tagArgs));
                                cmd.tagArgs = arg;
                            }
                            break;

                            case Awaiting::Any:
                            {
                                fmt::ReportUnknownParameter(arg);
                                cmd.Error();
                            }
                            break;
                        }
                    }
                }

                // optional normalization for filters
                if (!(cmd.flags & Command::caseSensitive))
                {
                    cmd.suiteArgs = fmt::ToLower(cmd.suiteArgs);
                    cmd.tagArgs = fmt::ToLower(cmd.tagArgs);
                }

                return cmd;
            }

            static auto ParseNames(const std::string& arg) -> std::vector<std::string>
            {
                auto sstr = std::stringstream{arg};
                auto res = std::vector<std::string>{};

                while (sstr.good())
                {
                    auto name = std::string{};
                    std::getline(sstr, name, ',');
                    res.push_back(name);
                }

                for (auto& name : res)
                {
                    fmt::Trim(name);
                }

                return res;
            }
        };
    };

    // get static CLI instance
    auto GetCLI() -> CLI&
    {
        static CLI reg;
        return reg;
    }

    // REGISTRY ---------------------------------------------------------------

    class Registry
    {
    public:
        // setup, teardown, tags
        class TestBase
        {
        public:
            // TODO: change init to {} (see telegram)
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
            auto Test(
                const std::string& name,
                const TestCase& test) -> TestCase&
            {
                tests.emplace_back(name, test);
                return tests.back().second;
            }

            auto Test(const nm::TestData& data) -> TestSuite&
            {
                tests.emplace_back(data.name, TestCase(
                    data.tags, data.func, data.setup, data.teardown));
                return *this;
            }

            // add an empty test (without test fn)
            auto Test(const std::string& name) -> TestCase&
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
            return suites[suite].Test(name, test);
        }

        // get all tests
        [[nodiscard]]
        auto AllTests() const -> const std::unordered_map<std::string, TestSuite>&
        {
            return suites;
        }

        // run tests with optional filtering
        auto Run(const CLI::Query& query) -> void
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

            auto& newTest = suite.Test(testName, TestCase{});
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
                  Summary&     summary) -> void
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

                    return;
                }

                if (const auto* test = dynamic_cast<const TestCase*>(testOrSuite))
                {
                    switch (funcType)
                    {
                        case FuncType::Setup:
                        {
                            if (test->TestBase::Setup())
                                test->TestBase::Setup()();
                            return;
                        }

                        case FuncType::Teardown:
                        {
                            if (test->TestBase::Teardown())
                                test->TestBase::Teardown()();
                            return;
                        }

                        case FuncType::Test:
                        {
                            ++summary.total;

                            if (!test->Func())
                            {
                                fmt::ReportMissingTest(name);
                                return;
                            }

                            const auto res = test->Func()();
                            fmt::ReportResult(name, res);
                            if (res)
                            {
                                ++summary.passed;
                                return;
                            }
                            summary.failed.emplace_back(name);
                        }
                    }
                }
            }
            catch (const std::exception& e)
            {
                fmt::ReportException(funcType, name, e.what());
                summary.errors.emplace_back(name);
            }
            catch (...)
            {
                fmt::ReportException(funcType, name);
                summary.errors.emplace_back(name);
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
            if (!name.empty()) GetRegistry().LastTest(&(GetRegistry().LastSuite()->Test(name)));
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
             Result (* func) ()>
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
              Result (* func) ()>
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
    auto Suite(const std::string& name) -> impl::Registry::TestSuite&
    {
        using namespace impl;
        return GetRegistry().GetSuite(name);
    }

    // get the registry
    auto Registry() -> impl::Registry&
    {
        return impl::GetRegistry();
    }

    // get the cli
    // TODO: REMOVE ON RELEASE
    auto CLI() -> impl::CLI&
    {
        return impl::GetCLI();
    }

    // run all tests with optional filtering
    auto Run(const int argc = 1, char** argv = nullptr) -> void
    {
        const auto query = impl::GetCLI().Query();
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