#include "ut.hpp"

int main(int argc, const char** argv) {
    using namespace boost::ut;

    run_cfg c {
        .report_errors = true,
        .argc = argc,
        .argv = argv,
    };

    return cfg<override>.run(c);
}