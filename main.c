#include "metacz.h"

func_ref_t
f_add_nums(cz_t *cz)
{
    cz_func_begin(cz);
        ref_t a = cz_func_in(cz, CZ_BASIC_TYPE(Int));
        ref_t b = cz_func_in(cz, CZ_BASIC_TYPE(Int));
    /**/
        cz_func_out(cz, CZ_BASIC_TYPE(Int));
    /**/
        CZ_LOAD(a); CZ_LOAD(b); CZ_ADD();
    return cz_func_end(cz);
}

func_ref_t
f_do_thing(cz_t *cz)
{
    cz_func_begin(cz);
        ref_t in = cz_func_in(cz, CZ_BASIC_TYPE(Int));
    /**/
        cz_func_out(cz, CZ_BASIC_TYPE(Int));
    /**/
        {
            scope_ref_t _scope = cz_scope_begin(cz);
            frame_ref_t __is_small = cz_scope_frame(cz);
        /**/
            CZ_LOAD(in);
            CZ_LOAD_IMM(5);
            CZ_JMP(Lt, __is_small);
                CZ_LOAD_IMM(1);
            CZ_JMP_END(Uc, _scope);
            CZ_LINK(__is_small);
                CZ_LOAD_IMM(0);
            CZ_END();
        }
    return cz_func_end(cz);
}

i32
main(void)
{
    cz_t cz = {0};

    func_ref_t proc_index   = f_add_nums(&cz);
    func_ref_t proc_index_2 = f_do_thing(&cz);

    cz_debug_dump(&cz);

    printf("\\_/\n V\n");
    return 0;
}
