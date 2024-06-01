#include "interpreter.h"

static void
vm_align_memory(vm_compiler_t *compiler, u32 alignment)
{
    ASSERT(alignement <= 16); // All we care about for now.

    u32 remainder = compiler->allocated_memory % alignment;

    if (remainder != 0) {
        compiler->allocated_memory += alignment - remainder;
    }
}

static vm_allocation_t
vm_type_to_allocation(cz_t *cz, type_ref_t type_ref)
{
    (void)cz;

    // TODO: Allow for all the types.
    ASSERT(type_ref.tag == data_type_Basic);
    ASSERT(type_ref.index_for_tag == data_basic_Int);

    return (vm_allocation_t) {
        .alignment = _Alignof(i32),
        .size      = sizeof(i32),
    };
}

static vm_object_t
vm_alloc_object(vm_compiler_t *compiler, cz_t *cz, type_ref_t type_ref)
{
    u32 prev_sp = compiler->allocated_memory;

    vm_allocation_t allocation = vm_type_to_allocation(cz, type_ref);

    vm_align_memory(compiler, allocation.alignment);

    vm_object_t object = {
        .prev_mem_off = prev_sp,
        .base_offset  = compiler->allocated_memory,
        .alignment    = allocation.alignment,
        .size         = allocation.size,
    };

    compiler->allocated_memory += allocation.size;

    return object;
}

u32
vm_compile(vm_t *vm, vm_compiler_t *compiler, cz_t *cz, func_ref_t func_ref)
{
    u32 code_offset = vm->code.count;

    abs_func_t *func = cz->abs_funcs.data + func_ref.func_index;

    u32 object_offset = compiler->objects.count;

    // TODO: Closure base pointers.

    u32 input_offset = compiler->objects.count;

    for (u32 i = 0; i < func->in_count; ++i) {
        type_ref_t type = cz->abs_func_ins.data[func->in_offset + i];
        dck_stretchy_push(compiler->objects, vm_alloc_object(compiler, cz, type));
    }

    u32 variable_offset = compiler->objects.count;

    for (u32 i = 0; i < func->var_count; ++i) {
        type_ref_t type = cz->abs_func_vars.data[func->var_offset + i];
        dck_stretchy_push(compiler->objects, vm_alloc_object(compiler, cz, type));
    }

    u32 eval_offset = compiler->objects.count;

    for (u32 i = 0; i < func->code_count; ++i) {
        abs_code_t code = cz->abs_code.data[func->code_offset + i];

        switch (code.inst) {
            case abs_inst_Add: /* fallthrough */
            case abs_inst_Sub: {
                ASSERT(eval_offset + 2 <= compiler->allocated_memory);
                
                vm_object_t object_r = compiler->objects.data[--(compiler->objects.count)];
                ASSERT(object_r.type_ref.tag == data_type_Basic); // TODO:
                ASSERT(object_r.type_ref.index_for_tag == data_basic_Int);

                vm_object_t object_l = compiler->objects.data[--(compiler->objects.count)];
                ASSERT(object_l.type_ref.tag == data_type_Basic); // TODO:
                ASSERT(object_l.type_ref.index_for_tag == data_basic_Int);

                dck_stretchy_push(compiler->instruction_offsets, vm->code.count);

                if (code.inst == abs_inst_Add) {
                    dck_stretchy_push(vm->code, vm_inst_AddInt);
                }
                else {
                    dck_stretchy_push(vm->code, vm_inst_SubInt);
                }

                dck_stretchy_push(compiler->objects, object_r);     // TODO: Depends on the type.
                compiler->allocated_memory = object_r.prev_mem_off; // TODO: Depends on the type.
            } break;

            case abs_inst_LoadIn: /* fallthrough */
            case abs_inst_LoadVar: {
                vm_object_t object;

                // TODO: Make work for closures.
                //       - Will require completely different code.
                if (code.inst == abs_inst_LoadIn) {
                    object = compiler->objects.data[input_offset + code.value - func->in_base];
                }
                else {
                    object = compiler->objects.data[variable_offset + code.value - func->var_base];
                }

                dck_stretchy_push(compiler->instruction_offsets, vm->code.count);

                u32 prev_mem_off = compiler->allocated_memory;

                u32 remainder = compiler->allocated_memory % object.alignment;
                if (remainder != 0) {
                    dck_stretchy_push(vm->code, vm_inst_AddSP);
                    dck_stretchy_push(vm->code, remainder);
                    compiler->allocated_memory += remainder;
                }

                dck_stretchy_push(vm->code, vm_inst_Load);
                dck_stretchy_push(vm->code, object.base_offset);
                dck_stretchy_push(vm->code, object.size);

                dck_stretchy_push(compiler->objects, (vm_object_t) {
                    .prev_mem_off = prev_mem_off,
                    .base_offset = compiler->allocated_memory,
                    .alignment = object.alignment,
                    .size = object.size,
                });

                compiler->allocated_memory += object.size;
            } break;

            case abs_inst_LoadImm: {
                ASSERT(i + 1 < func->code_count);

                u32 imm_index = cz->abs_code.data[func->code_offset + ++i].index;
                immediate_t immediate = cz->immediates.data[imm_index];

                u32 prev_mem_off = compiler->allocated_memory;

                u32 remainder = compiler->allocated_memory % object.alignment;
                if (remainder != 0) {
                    dck_stretchy_push(vm->code, vm_inst_AddSP);
                    dck_stretchy_push(vm->code, remainder);
                    compiler->allocated_memory += remainder;
                }
                dck_stretchy_push(compiler->objects, (vm_object_t) {
                    .prev_mem_off = prev_mem_off,
                    .base_offset = compiler->allocated_memory,
                    .alignment = object.alignment,
                    .size = object.size,
                });

            } break;
        }
    }
}

void
vm_execute(vm_t *vm, cz_t *cz, u32 code_offset)
{
}
