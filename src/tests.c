#include "metacz.h"
#include "interpreter.h"

func_ref_t
f_add_example(cz_t *cz)
{
    cz_func_begin(cz);
        ref_t a = cz_func_in(cz, CZ_BASIC_VAR(Int));
        ref_t b = cz_func_in(cz, CZ_BASIC_VAR(Int));
    /**/
        cz_func_out(cz, CZ_BASIC_VAR(Int));
    /**/
        CZ_LOAD(a); CZ_LOAD(b); CZ_ADD();
    return cz_func_end(cz);
}

func_ref_t
f_jmp_example(cz_t *cz)
{
    cz_func_begin(cz);
        ref_t in = cz_func_in(cz, CZ_BASIC_VAR(Int));
    /**/
        cz_func_out(cz, CZ_BASIC_VAR(Int));
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

func_ref_t
f_add_nums(cz_t *cz)
{
    type_ref_t arr_5_t = cz_make_type_array(cz, CZ_BASIC_TYPE(Int), 5);

    cz_func_begin(cz);
        ref_t base = cz_func_in(cz, CZ_BASIC_VAR(Int));
    /**/
        cz_func_out(cz, CZ_VAR(arr_5_t));
    /**/
        ref_t arr_5 = cz_func_var(cz, CZ_VAR(arr_5_t));
        ref_t index = cz_func_var(cz, CZ_BASIC_VAR(Int));
    /**/
        CZ_LOAD_IMM(0); CZ_STORE(index);

        {
            scope_ref_t _scope = cz_scope_begin(cz);
            frame_ref_t __loop = cz_scope_frame(cz);
        /**/
            CZ_LINK(__loop);
                CZ_LOAD(index); CZ_LOAD_REF(arr_5); CZ_LENGTH(); CZ_JMP_END(Ge, _scope);

                CZ_LOAD(base); CZ_LOAD(index); CZ_ADD();
                CZ_LOAD(index); CZ_LOAD_REF(arr_5); CZ_READ();
                CZ_STORE_REF();

                CZ_LOAD(index); CZ_LOAD_IMM(1); CZ_ADD();
                CZ_STORE(index);

                CZ_JMP(Uc, __loop);
            CZ_END();
        }

        CZ_LOAD(arr_5);
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

#define TEST_FAILURE(fmt_m, ...) \
    do { \
        printf("[" ANSI_RED "FAILURE" ANSI_RESET "] %s() %s:%d\n", __func__, __FILE__, __LINE__); \
        printf("  (" fmt_m ")\n", __VA_ARGS__); \
        exit(1); \
    } while (0)

#define TEST_SUCCESS() \
    do { \
        printf("[" ANSI_GREEN "SUCCESS" ANSI_RESET "] %s() %s:%d\n", __func__, __FILE__, __LINE__); \
    } while (0)

i32
main(void)
{
    cz_t cz = {0};

    func_ref_t add_func = f_add_example(&cz);
    func_ref_t jmp_func = f_jmp_example(&cz);
    func_ref_t loop_func = f_add_nums(&cz);

    vm_t vm = {0};
    vm_compiler_t compiler = {0};

    vm_func_ref_t add_f = vm_compile(&vm, &compiler, &cz, add_func);

    i32 res;

    vm_clear(&vm);
    VM_PUSH(&vm, i32, &(i32) { 5 });
    VM_PUSH(&vm, i32, &(i32) { 3 });
    vm_execute(&vm, &cz, add_f.code_offset);
    res = *VM_GET(&vm, i32);
    TEST(res == 8);

    vm_clear(&vm);
    VM_PUSH(&vm, i32, &(i32) { -1 });
    VM_PUSH(&vm, i32, &(i32) { 4 });
    vm_execute(&vm, &cz, add_f.code_offset);
    res = *VM_GET(&vm, i32);
    TEST(res == 3);

    vm_func_ref_t jmp_f = vm_compile(&vm, &compiler, &cz, jmp_func);

    vm_clear(&vm);
    VM_PUSH(&vm, i32, &(i32) { 8 });
    vm_execute(&vm, &cz, jmp_f.code_offset);
    res = *VM_GET(&vm, i32);
    TEST(res == 1);

    vm_func_ref_t loop_f = vm_compile(&vm, &compiler, &cz, loop_func);

    vm_clear(&vm);
    i32 base = 3;
    VM_PUSH(&vm, i32, &(i32) { 3 });
    vm_execute(&vm, &cz, loop_f.code_offset);
    i32 *data = VM_GET_ARR(&vm, i32, 5);

    for (i32 i = 0; i < 5; ++i) {
        if (data[i] != base + i) {
            TEST_FAILURE("data[%d] == %d", i, base + i);
        }
    }
    TEST_SUCCESS();

    printf("\\_/\n V\n");
    return 0;
}
