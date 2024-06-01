#include "interpreter.h"

#include <string.h>

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

static vm_object_t
vm_push_type(vm_t *vm, vm_compiler_t *compiler, cz_t *cz, type_ref_t type_ref)
{
    u32 prev_sp = compiler->allocated_memory;

    vm_allocation_t allocation = vm_type_to_allocation(cz, type_ref);

    u32 remainder = compiler->allocated_memory % allocation.alignment;
    if (remainder != 0) {
        dck_stretchy_push(vm->code, vm_inst_IncSP);
        dck_stretchy_push(vm->code, remainder);
        compiler->allocated_memory += remainder;
    }

    vm_object_t object = {
        .prev_mem_off = prev_sp,
        .base_offset  = compiler->allocated_memory,
        .alignment    = allocation.alignment,
        .size         = allocation.size,
    };

    dck_stretchy_push(compiler->objects, object);

    compiler->allocated_memory += allocation.size;

    return object;
}

static vm_object_t
vm_pop_object(vm_t *vm, vm_compiler_t *compiler, cz_t *cz)
{
    vm_object_t object = compiler->objects.data[--(compiler->objects.count)];
    compiler->allocated_memory = object.prev_mem_off;
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

        dck_stretchy_push(compiler->instruction_offsets, vm->code.count);

        switch (code.inst) {
            case abs_inst_Add: /* fallthrough */
            case abs_inst_Sub: {
                ASSERT(eval_offset + 2 <= compiler->allocated_memory);
                
                vm_object_t object_r = vm_pop_object(vm, compiler, cz);
                ASSERT(object_r.type_ref.tag == data_type_Basic); // TODO:
                ASSERT(object_r.type_ref.index_for_tag == data_basic_Int);

                vm_object_t object_l = vm_pop_object(vm, compiler, cz);
                ASSERT(object_l.type_ref.tag == data_type_Basic); // TODO:
                ASSERT(object_l.type_ref.index_for_tag == data_basic_Int);

                if (code.inst == abs_inst_Add) {
                    dck_stretchy_push(vm->code, vm_inst_AddInt);
                }
                else {
                    dck_stretchy_push(vm->code, vm_inst_SubInt);
                }

                vm_object_t object = vm_push_type(vm, compiler, cz, object_l.type_ref);
            } break;

            case abs_inst_LoadIn: /* fallthrough */
            case abs_inst_LoadVar: {
                // TODO: Make work for closures.

                vm_object_t object;

                if (code.inst == abs_inst_LoadIn) {
                    object = compiler->objects.data[input_offset + code.value - func->in_base];
                }
                else {
                    object = compiler->objects.data[variable_offset + code.value - func->var_base];
                }

                vm_object_t new_object = vm_push_type(vm, compiler, cz, object.type_ref);

                dck_stretchy_push(vm->code, vm_inst_Load);
                dck_stretchy_push(vm->code, new_object.base_offset);
                dck_stretchy_push(vm->code, new_object.size);
            } break;

            case abs_inst_LoadImm: {
                ASSERT(i + 1 < func->code_count);

                u32 imm_index = cz->abs_code.data[func->code_offset + ++i].index;
                immediate_t immediate = cz->immediates.data[imm_index];

                vm_object_t object = vm_push_type(vm, compiler, cz, immediate.type);

                dck_stretchy_push(vm->code, vm_inst_LoadImm);
                dck_stretchy_push(vm->code, object.size);

                u32 imm_inst_size = (object.size + sizeof(vm_inst_t) - 1) / sizeof(vm_inst_t);
                dck_stretchy_reserve(vm->code, imm_inst_size);

                u8 *imm_ptr = (u8 *)(cz->imm_data.data + immediate.data_offset);

                memcpy(vm->code.data + vm->code.count, imm_ptr, object.size);

                vm->code.count += imm_inst_size;
            } break;
        }
    }
}

void
vm_init(vm_t *vm, u32 code_offset)
{
    vm->ip = code_offset;
    vm->sp = 0;
    vm->bp = 0;
    vm->memory.count = 0;
}

void
vm_step(vm_t *vm, cz_t *cz)
{
    ASSERT(vm->ip < vm->code.count);

    vm_inst_t inst = vm->code.data[vm->ip];

    if (inst == vm_inst_Halt)
        return;

    vm->ip++;

    switch (inst) {
        case vm_inst_IncSP: {
            ASSERT(vm->ip < vm->code.count);
            u32 amount = vm->code.data[vm->ip++];
            vm->sp += amount;
        } break;

        case vm_inst_AddInt: {
            ASSERT(vm->ip + 1 < vm->code.count);
        } break;
    }
}

b32
vm_is_running(vm_t *vm)
{
    ASSERT(vm->ip < vm->code.count);

    return vm->code.data[vm->ip] != vm_inst_Halt;
}

void
vm_execute(vm_t *vm, cz_t *cz, u32 code_offset)
{
    vm_init(vm, code_offset);

    while (vm_is_running(vm)) {
        vm_step(vm, cz);
    }
}
