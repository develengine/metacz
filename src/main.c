#include "metacz.h"
#include "interpreter.h"

void
func_call_example(cz_t *cz, vm_t *vm, vm_compiler_t *compiler)
{
    CZ_FUNC (func_aux) {
        CZ_IN  (a, CZ_BASIC_TYPE(Int));
        CZ_OUT (CZ_BASIC_TYPE(Int));
    /**/
        CZ_LOAD_IMM(123); CZ_LOAD(a); CZ_SUB();
    }

    CZ_FUNC (func_1) {
        CZ_IN  (a, CZ_BASIC_TYPE(Int));
        CZ_IN  (b, CZ_BASIC_TYPE(Int));
        CZ_OUT (CZ_BASIC_TYPE(Int));
    /**/
        CZ_LOAD(a); CZ_CALL(func_aux);
        CZ_LOAD(b);
        CZ_ADD();
    }

    vm_func_ref_t func_1_vm = vm_compile(vm, compiler, cz, func_1);

    i32 res;

    vm_clear(vm);
    VM_PUSH(vm, i32, &(i32) { 5 });
    VM_PUSH(vm, i32, &(i32) { 3 });
    vm_call(vm, compiler, cz, func_1_vm);
    res = *VM_GET(vm, i32);
    printf("res = %d\n", res);
}

i32
main(void)
{
    cz_t cz = {0};
    vm_t vm = vm_create();
    vm_compiler_t compiler = {0};

    func_call_example(&cz, &vm, &compiler);

    printf("\\_/\n V\n");
    return 0;
}
