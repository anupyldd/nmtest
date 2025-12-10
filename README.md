# nmtest
**nmtest** is a single-module macro free testing library written in modern C++.
It provides a set of standalone functions for various checks, as well as an API
to organize them into suites and run with optional filtering.

Why macro-free? See [my blog post]() detailing the motivation and implementation details.

The library itself can be found in the `nmtest.ixx` file in the `src/` folder. You can
just copy it to your project. `C++23` is required.

*Note: this library was created mainly as an experiment on what a macro-free testing
library in C++ could look like. While it’s functional, it’s intentionally minimal 
and not intended for production use.*

### Minimal example

```c++
Test("Math", "Addition")
.Func([]{ return Equal(2+2, 4); })
```

### Checks

All checks return a `Result` object that is convertible to `bool` 
and enables easily chaining multiple checks as will be shown later.

- `Equal(a, b)` (works for floating-point types as well)
- `NotEqual(a, b)`
- `Throws(func)`
- `DoesNotThrow(func)`
- `Null(val)`
- `NotNull(val)`
- `True(val)`
- `False(val)`

All checks also support custom error messages 
(e.g. `Equal(2 * 2, 5, "that's not how it works");`).

### Tests

#### Creation

Tests can be created in one of the following ways. This is due to the experimental nature
of the library, some methods are included only to demonstrate alternative ways test registration
could be implemented.

A test is automatically registered upon creation - no need for manual registration.
For each test there are only 3 required fields: *suite name, test name and test function*.
Optional fields are: *tags, setup function, teardown function*.

1) Single test. *(Best when you need to create a single test and can do it in a function context)*
    ```c++
    Test("suite name", "test name")
    .Setup([]{ /* optional setup function */ })
    .Teardown([]{ /* optional teardown function */ })
    .Tags({ /* optional tag list */ })
    .Func([]{ return Equal(1, 2); });
    ```

2) Suite with any number of tests. *(Best when you need to register several tests into a single suite
at once, and can do it in a function context)*
    ```c++
    Suite("suite name")
    .Setup([]{ /* optional suite setup*/ })
    .Teardown([]{ /* optional suite teardown*/ })
    .Test({
        .name = "test 1",
        .func = []{ return Equal(0,9); }
    })
    .Test({
        .name = "test 2",
        .tags = { /* optional tag list */ }
        .func = []{ return Equal(0,8); }
    });
    ```
3) Single test through object creation. Requires naming each individual test object, 
but allows creating tests in global scope, outside any function, through static initialization
that happens before `main()`. *(Preferred when you need to create a test in a global context)*
    ```c++
    TestS s2{
        "suite name",
        "test name",
        []{ return Equal(1,2); }
    };
    ```
4) Single test through template initialization. Works in global scope like *Method 3* 
and does not require naming each test. 
Stricter syntax and requires the test to be in a source file (not header).
*(Left in mainly because it's an interesting trick to avoid naming each individual test object.
Can be used, but is quite finicky)*
   ```c++
   template class TestT<
       "suite name",
       "test name",
       []{ return Equal(1,2); }>;
   ```

#### Test functions

Note that every test function must return a `Result` object. 
Built-in checks already do this, so you can just write `return CheckName(args);`.

You can also chain any number of checks with the `&` operator, so in a single
test function you can write:
```c++
return
  Equal   (3, 5)
& Equal   (1, 1)
& NotEqual(2, 2, "custom message");
```
The checks do *not* short-circuit. Even if the first one fails, all the others are still
executed. This is done to collect as much fail information in one test run.

The returned `Result` object also contains information on all failed tests,
including test location and specific checks that failed.
So the example above will output:
```
[X  FAIL] TestName (main.cpp:100):
          - Equal(3:5) | main.cpp:101
          - NotEqual(2:2) | main.cpp:103 | custom message
```
*Note: this is shown only if the `--verbose` option is enabled.*

In case you need to save the results of checks in different places of the test function,
you can also write (the output will be equivalent, save for some changes in line numbers):
```c++
auto res = Equal(3, 5);
// some code
res & Equal(1, 1);
// some more code
res & NotEqual(2, 2, "custom message");
return res;
```

#### Summary

All the test results are saved to output a convenient summary in the end.
Summary example:

```
[SUMMARY] Total: 3; Passed: 1; Failed: 2; Errors: 0
          Failed tests:
          - Subtraction (main.cpp:139)
          - Multiplication (main.cpp:167)
```

The `Errors` occur when there's an unhandled exception in the test, setup, or teardown function.

#### Running and Filtering

To run registered tests you need to call the `Run()` function.
Without any parameters, it runs all tests in all suites that were added
before the call to `Run()`.

You can optionally filter the tests by suite and tag by passing the 
command-line arguments to the function and specifying the filters through
the command line: `Run(argc, argv);`.

### CLI

The library offers simple CLI for controlling the execution of the tests.
```
Usage:
test [options]

Options:
-s, --suite <names>         Comma-separated list of suite names to run.
-t, --tag <names>           Comma-separated list of tag names to run.
-l, --list                  List matching tests without running them.
-c, --case_sensitive        Make name matching case-sensitive.
-h, --help                  Show this help message.

Example:
test -s "core,math" -t "fast"
Run all tests from suites "core" or "math" that have tag "fast".

Notes:
- Multiple suites or tags are treated as OR:
  -s "core,math"     runs tests in either suite.
  -t "fast,slow"     runs tests tagged fast OR slow.
- Names are case-insensitive unless --case_sensitive is used.
```