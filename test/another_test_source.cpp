
#include "names.h"
import nm;

using namespace nm;

template class TestT<
    "Suite 2",
    "TemplateTest2 (test_source.cpp:18)",
    []{ return Equal(22,33); }>;