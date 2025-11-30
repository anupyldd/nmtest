import nm;

using namespace nm;

template class TestT<
    "SourceFile",
    "TestSourceFile",
    []{ return Equal(22,33); }>;