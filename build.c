#define BLD_IMPLEMENTATION
#include "core/bld.h"

#include "core/utils.h"

i32
tests(i32 argc, char *argv[])
{
    char *output = "tests";

    bld_sa_t cc = {0};
    BLD_SA_PUSH(cc, "src/tests.c", "src/metacz.c", "src/interpreter.c");
    BLD_SA_PUSH(cc, "-I.", "-Isrc", "-o", output, BLD_WARNINGS);
    BLD_SA_PUSH(cc, "-g");
    BLD_SA_PUSH(cc, "-D_DEBUG");

    u32 res = bld_cc_params((const char **)cc.data, cc.count);
    if (res != 0)
        return res;

    return bld_run_program(output);
}

i32
main(i32 argc, char *argv[])
{
    BLD_TRY_REBUILD_SELF(argc, argv);

    if (bld_contains("test", argc, argv))
        return tests(argc, argv);

    char *output = "model";

    bld_sa_t cc = {0};
    BLD_SA_PUSH(cc, "src/main.c", "src/metacz.c", "src/interpreter.c");
    BLD_SA_PUSH(cc, "-I.", "-Isrc", "-o", output, BLD_WARNINGS);
    if (bld_contains("debug", argc, argv)) {
        BLD_SA_PUSH(cc, "-g");
        BLD_SA_PUSH(cc, "-D_DEBUG");
    }

    u32 res = bld_cc_params((const char **)cc.data, cc.count);
    if (res != 0)
        return res;

    if (bld_contains("run", argc, argv))
        return bld_run_program(output);

    return 0;
}
