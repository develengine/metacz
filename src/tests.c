#include "metacz.h"
#include "interpreter.h"

func_ref_t
f_add_example(cz_t *cz)
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
f_jmp_example(cz_t *cz)
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
            CZ_LOAD(in); CZ_LOAD_IMM(5); CZ_JMP(Lt, __is_small);
                CZ_LOAD_IMM(1);
                CZ_JMP_END(Uc, _scope);
            CZ_LINK(__is_small);
                CZ_LOAD_IMM(0);
            CZ_END();
        }
    return cz_func_end(cz);
}

#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_RESET   "\x1b[0m"

#define TEST(...) \
    do { \
        if (__VA_ARGS__) { \
            printf("[" ANSI_GREEN "SUCCESS" ANSI_RESET "] %s() %s:%d\n", __func__, __FILE__, __LINE__); \
        } \
        else { \
            printf("[" ANSI_RED "FAILURE" ANSI_RESET "] %s() %s:%d\n", __func__, __FILE__, __LINE__); \
            printf("  (%s)\n", #__VA_ARGS__); \
            exit(1); \
        } \
    } while (0)

i32
main(void)
{
    cz_t cz = {0};

    func_ref_t add_func = f_add_example(&cz);
    func_ref_t jmp_func = f_jmp_example(&cz);

    vm_t vm = {0};
    vm_compiler_t compiler = {0};

    u32 add_code_offset = vm_compile(&vm, &compiler, &cz, add_func);

    vm_clear(&vm);
    VM_PUSH(&vm, i32, &(i32) { 5 });
    VM_PUSH(&vm, i32, &(i32) { 3 });
    vm_execute(&vm, &cz, add_code_offset);
    int res = *VM_GET(&vm, i32);
    TEST(res == 8);

    printf("\\_/\n V\n");
    return 0;
}
