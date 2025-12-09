
#include "names.h"
import nm;

using namespace nm;

template class TestT<
    "Suite 2",
    "TemplateTest2 (another_test_source.cpp:9)",
    []{ return Equal(22,33); }>;