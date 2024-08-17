#include "interpreter.h"

#include <string.h>

static void
vm_align_memory(vm_compiler_t *compiler, u32 alignment)
{
    ASSERT(alignment <= 16); // All we care about for now.

    u32 remainder = compiler->allocated_memory % alignment;

    if (remainder != 0) {
        compiler->allocated_memory += alignment - remainder;
    }
}

static vm_allocation_t
vm_type_to_allocation(cz_t *cz, type_ref_t type)
{
    if (type.tag == data_type_Basic) {
        ASSERT(type.index_for_tag == data_basic_Int);

        return (vm_allocation_t) {
            .alignment = _Alignof(i32),
            .size      = sizeof(i32),
        };
    }

    if (type.tag == data_type_Array) {
        type_array_t array_type = cz->array_types.data[type.index_for_tag];
        vm_allocation_t elem_alloc = vm_type_to_allocation(cz, array_type.type);

        return (vm_allocation_t) {
            .alignment = elem_alloc.alignment,
            .size      = elem_alloc.size * array_type.length,
        };
    }

    UNREACHABLE();
}

typedef struct
{
    u32 offset, length;
} vm_array_ref_t;

static vm_allocation_t
vm_var_to_allocation(cz_t *cz, variable_t var)
{
    if (var.is_reference) {
        if (var.type.tag == data_type_Array) {
            return (vm_allocation_t) {
                .alignment = _Alignof(vm_array_ref_t),
                .size      = sizeof(vm_array_ref_t),
            };
        }

        return (vm_allocation_t) {
            .alignment = _Alignof(u32),
            .size      = sizeof(u32),
        };
    }

    return vm_type_to_allocation(cz, var.type);
}

static vm_object_t
vm_alloc_object(vm_compiler_t *compiler, cz_t *cz, variable_t var)
{
    u32 prev_sp = compiler->allocated_memory;

    vm_allocation_t allocation = vm_var_to_allocation(cz, var);

    vm_align_memory(compiler, allocation.alignment);

    vm_object_t object = {
        .prev_mem_off = prev_sp,
        .base_offset  = compiler->allocated_memory,
        .alignment    = allocation.alignment,
        .size         = allocation.size,

        .var          = var,
    };

    compiler->allocated_memory += allocation.size;

    return object;
}

static u32
vm_align(u32 value, u32 alignment)
{
    u32 remainder = value % alignment;
    if (remainder != 0) {
        value += alignment - remainder;
    }
    return value;
}

static vm_object_t *
vm_push_var(vm_t *vm, vm_compiler_t *compiler, cz_t *cz, variable_t var)
{
    u32 prev_sp = compiler->allocated_memory;

    vm_allocation_t allocation = vm_var_to_allocation(cz, var);

    u32 remainder = compiler->allocated_memory % allocation.alignment;
    if (remainder != 0) {
        dck_stretchy_push(vm->code, vm_inst_IncSP);
        dck_stretchy_push(vm->code, allocation.alignment - remainder);
        compiler->allocated_memory += allocation.alignment - remainder;
    }

    vm_object_t object = {
        .prev_mem_off = prev_sp,
        .base_offset  = compiler->allocated_memory,
        .alignment    = allocation.alignment,
        .size         = allocation.size,

        .var          = var,
    };

    dck_stretchy_push(compiler->objects, object);

    compiler->allocated_memory += allocation.size;

    return compiler->objects.data + compiler->objects.count - 1;
}

static vm_object_t
vm_pop_object(vm_t *vm, vm_compiler_t *compiler, cz_t *cz)
{
    (void)cz;

    vm_object_t object = compiler->objects.data[--(compiler->objects.count)];
    u32 object_end = object.base_offset + object.size;

    if (compiler->allocated_memory > object_end) {
        i32 diff = object_end - compiler->allocated_memory;
        dck_stretchy_push(vm->code, vm_inst_IncSP);
        dck_stretchy_push(vm->code, *(u32 *)(&diff));
        compiler->allocated_memory = (u32)((i32)compiler->allocated_memory + diff);
    }

    compiler->allocated_memory -= object.size;
    return object;
}

vm_func_ref_t
vm_compile(vm_t *vm, vm_compiler_t *compiler, cz_t *cz, func_ref_t func_ref)
{
    compiler->allocated_memory   = 0;

    u32 object_offset       = compiler->objects.count;
    u32 jump_patches_offset = compiler->jump_patches.count;
    u32 label_offset        = compiler->labels.count;

    u32 code_offset = vm->code.count;

    abs_func_t *func = cz->abs_funcs.data + func_ref.func_index;

    u32 input_offset = compiler->objects.count;

    for (u32 i = 0; i < func->in_count; ++i) {
        variable_t var = cz->abs_func_ins.data[func->in_offset + i];
        vm_object_t object = vm_alloc_object(compiler, cz, var);
        dck_stretchy_push(compiler->objects, object);
    }

    // TODO: return address + base pointer + Closure base pointers.

    u32 variable_offset = compiler->objects.count;
    u32 variable_memory_offset = compiler->allocated_memory;

    for (u32 i = 0; i < func->var_count; ++i) {
        variable_t var = cz->abs_func_vars.data[func->var_offset + i];
        vm_object_t object = vm_alloc_object(compiler, cz, var);
        dck_stretchy_push(compiler->objects, object);
    }

    u32 eval_offset = compiler->objects.count;
    u32 eval_memory_offset = compiler->allocated_memory;

    if (variable_memory_offset < eval_memory_offset) {
        dck_stretchy_push(vm->code, vm_inst_IncSP);
        dck_stretchy_push(vm->code, eval_memory_offset - variable_memory_offset);
    }

    for (u32 inst_index = 0; inst_index < func->code_count; ++inst_index) {
        abs_code_t code = cz->abs_code.data[func->code_offset + inst_index];

        vm_object_t object;
        vm_object_t object_r;
        vm_object_t object_l;

        switch (code.inst) {
            case abs_inst_Ret:
                continue;

            case abs_inst_Add: /* fallthrough */
            case abs_inst_Sub: {
                // NOTE: Types need to be tilable.

                ASSERT(eval_offset + 2 <= compiler->objects.count);

                object_r = vm_pop_object(vm, compiler, cz);
                ASSERT(object_r.var.is_reference == false);
                ASSERT(object_r.var.type.tag == data_type_Basic); // TODO:
                ASSERT(object_r.var.type.index_for_tag == data_basic_Int);

                dck_stretchy_push(vm->code, vm_inst_StoreInt);
                dck_stretchy_push(vm->code, 0);

                object_l = vm_pop_object(vm, compiler, cz);
                ASSERT(object_l.var.is_reference == false);
                ASSERT(object_l.var.type.tag == data_type_Basic); // TODO:
                ASSERT(object_l.var.type.index_for_tag == data_basic_Int);

                dck_stretchy_push(vm->code, vm_inst_StoreInt);
                dck_stretchy_push(vm->code, 1);

                if (code.inst == abs_inst_Add) {
                    dck_stretchy_push(vm->code, vm_inst_AddInt);
                }
                else {
                    dck_stretchy_push(vm->code, vm_inst_SubInt);
                }

                vm_push_var(vm, compiler, cz, object_l.var);

                dck_stretchy_push(vm->code, vm_inst_LoadInt);
                dck_stretchy_push(vm->code, 0);
            } continue;

            case abs_inst_LoadIn: /* fallthrough */
            case abs_inst_LoadVar: {
                // TODO: Make work for closures.

                ASSERT(inst_index + 1 < func->code_count);
                u32 local_index = cz->abs_code.data[func->code_offset + ++inst_index].index;

                if (code.inst == abs_inst_LoadIn) {
                    object = compiler->objects.data[input_offset + local_index - func->in_base];
                }
                else {
                    object = compiler->objects.data[variable_offset + local_index - func->var_base];
                }

                vm_push_var(vm, compiler, cz, object.var);

                dck_stretchy_push(vm->code, vm_inst_Load);
                dck_stretchy_push(vm->code, object.base_offset);
                dck_stretchy_push(vm->code, object.size);
            } continue;

            case abs_inst_LoadRefIn: /* fallthrough */
            case abs_inst_LoadRefVar: {
                // TODO: Make work for closures.

                ASSERT(inst_index + 1 < func->code_count);
                u32 local_index = cz->abs_code.data[func->code_offset + ++inst_index].index;

                if (code.inst == abs_inst_LoadIn) {
                    object = compiler->objects.data[input_offset + local_index - func->in_base];
                }
                else {
                    object = compiler->objects.data[variable_offset + local_index - func->var_base];
                }

                ASSERT(object.var.is_reference == false);

                vm_push_var(vm, compiler, cz, (variable_t) {
                    .type         = object.var.type,
                    .is_reference = true,
                });

                dck_stretchy_push(vm->code, vm_inst_RelToAbs);
                dck_stretchy_push(vm->code, object.base_offset);

                if (object.var.type.tag == data_type_Array) {
                    type_array_t array_type = cz->array_types.data[object.var.type.index_for_tag];

                    dck_stretchy_push(vm->code, vm_inst_LoadImm);
                    dck_stretchy_push(vm->code, sizeof(u32));
                    dck_stretchy_push(vm->code, array_type.length);
                }
            } continue;

            case abs_inst_LoadImm: {
                ASSERT(inst_index + 1 < func->code_count);

                u32 imm_index = cz->abs_code.data[func->code_offset + ++inst_index].index;
                immediate_t immediate = cz->immediates.data[imm_index];

                variable_t var = {
                    .type         = immediate.type,
                    .is_reference = false,
                };
                object = *vm_push_var(vm, compiler, cz, var);

                dck_stretchy_push(vm->code, vm_inst_LoadImm);
                dck_stretchy_push(vm->code, object.size);

                u32 imm_inst_size = (object.size + sizeof(vm_inst_t) - 1) / sizeof(vm_inst_t);
                dck_stretchy_reserve(vm->code, imm_inst_size);

                u8 *imm_ptr = (u8 *)(cz->imm_data.data + immediate.data_offset);

                memcpy(vm->code.data + vm->code.count, imm_ptr, object.size);

                vm->code.count += imm_inst_size;
            } continue;

            case abs_inst_StoreIn: /* fallthrough */
            case abs_inst_StoreVar: {
                // TODO: Make work for closures.

                ASSERT(inst_index + 1 < func->code_count);
                u32 local_index = cz->abs_code.data[func->code_offset + ++inst_index].index;

                if (code.inst == abs_inst_LoadIn) {
                    object = compiler->objects.data[input_offset + local_index - func->in_base];
                }
                else {
                    object = compiler->objects.data[variable_offset + local_index - func->var_base];
                }

                vm_object_t stack_object = vm_pop_object(vm, compiler, cz);

                dck_stretchy_push(vm->code, vm_inst_Store);
                dck_stretchy_push(vm->code, object.base_offset);
                dck_stretchy_push(vm->code, stack_object.size);
            } continue;

            case abs_inst_StoreRef: {
                ASSERT(eval_offset + 2 <= compiler->objects.count);

                object = vm_pop_object(vm, compiler, cz);
                ASSERT(object.var.is_reference == true);

                if (object.var.type.tag == data_type_Array) {
                    dck_stretchy_push(vm->code, vm_inst_IncSP);
                    i32 off = -4;
                    dck_stretchy_push(vm->code, *(u32 *)(&off));
                }

                dck_stretchy_push(vm->code, vm_inst_StoreAP);

                vm_object_t stack_object = vm_pop_object(vm, compiler, cz);

                dck_stretchy_push(vm->code, vm_inst_StoreAbs);
                dck_stretchy_push(vm->code, stack_object.size);
            } continue;

            case abs_inst_Deref: {
                ASSERT(eval_offset + 1 <= compiler->objects.count);

                object = vm_pop_object(vm, compiler, cz);
                ASSERT(object.var.is_reference == true);

                if (object.var.type.tag == data_type_Array) {
                    dck_stretchy_push(vm->code, vm_inst_IncSP);
                    i32 off = -4;
                    dck_stretchy_push(vm->code, *(u32 *)(&off));
                }

                dck_stretchy_push(vm->code, vm_inst_StoreAP);

                vm_allocation_t alloc = vm_type_to_allocation(cz, object.var.type);

                vm_push_var(vm, compiler, cz, (variable_t) {
                    .type = object.var.type,
                });

                dck_stretchy_push(vm->code, vm_inst_LoadAbs);
                dck_stretchy_push(vm->code, alloc.size);
            } continue;

            case abs_inst_ArrRead: {
                ASSERT(eval_offset + 2 <= compiler->objects.count);

                vm_object_t ref_obj = vm_pop_object(vm, compiler, cz);
                ASSERT(ref_obj.var.is_reference == true);
                ASSERT(ref_obj.var.type.tag == data_type_Array);

                dck_stretchy_push(vm->code, vm_inst_IncSP);
                i32 off = -4;
                dck_stretchy_push(vm->code, *(u32 *)(&off));

                dck_stretchy_push(vm->code, vm_inst_StoreAP);

                type_array_t array_type = cz->array_types.data[ref_obj.var.type.index_for_tag];

                vm_allocation_t element_alloc = vm_type_to_allocation(cz, array_type.type);

                vm_object_t index_obj = vm_pop_object(vm, compiler, cz);
                ASSERT(index_obj.var.is_reference == false);
                ASSERT(index_obj.var.type.tag == data_type_Basic);
                ASSERT(index_obj.var.type.index_for_tag == data_basic_Int);

                dck_stretchy_push(vm->code, vm_inst_IncMulAP);
                dck_stretchy_push(vm->code, element_alloc.size);

                vm_push_var(vm, compiler, cz, (variable_t) {
                    .type         = array_type.type,
                    .is_reference = true,
                });

                dck_stretchy_push(vm->code, vm_inst_LoadAP);
            } continue;

            case abs_inst_ArrLength: {
                ASSERT(eval_offset + 1 <= compiler->objects.count);

                vm_object_t ref_obj = vm_pop_object(vm, compiler, cz);
                ASSERT(ref_obj.var.is_reference == true);
                ASSERT(ref_obj.var.type.tag == data_type_Array);

                dck_stretchy_push(vm->code, vm_inst_MemMove);
                dck_stretchy_push(vm->code, compiler->allocated_memory);
                dck_stretchy_push(vm->code, compiler->allocated_memory + sizeof(u32));
                dck_stretchy_push(vm->code, sizeof(u32));

                dck_stretchy_push(vm->code, vm_inst_IncSP);
                i32 off = -4;
                dck_stretchy_push(vm->code, *(u32 *)(&off));

                vm_push_var(vm, compiler, cz, (variable_t) {
                    .type = (type_ref_t) {
                        .tag           = data_type_Basic,
                        .index_for_tag = data_basic_Int,
                    },
                });
            } continue;

            case abs_inst_Label: {
                ASSERT(inst_index + 1 < func->code_count);
                u32 label_index = cz->abs_code.data[func->code_offset + ++inst_index].index;

                for (u32 i = jump_patches_offset; i < compiler->jump_patches.count;) {
                    vm_patch_t patch = compiler->jump_patches.data[i];

                    if (patch.label_index == label_index) {
                        // Offset by one cuz the IP points to the next instruction, not current.
                        i32 rel_offset = (i32)(vm->code.count) - ((i32)patch.absolute_offset + 1);
                        vm->code.data[patch.absolute_offset] = *((u32*)&rel_offset);

                        compiler->jump_patches.data[i] = compiler->jump_patches.data[--(compiler->jump_patches.count)];
                    }
                    else {
                        ++i;
                    }
                }

                dck_stretchy_push(compiler->labels, (vm_patch_t) {
                    .label_index     = label_index,
                    .absolute_offset = vm->code.count,
                });
            } continue;

            case abs_inst_JmpGe: /* fallthrough */
            case abs_inst_JmpLe: /* fallthrough */
            case abs_inst_JmpGt: /* fallthrough */
            case abs_inst_JmpLt: /* fallthrough */
            case abs_inst_JmpNe: /* fallthrough */
            case abs_inst_JmpEq:
                object_l = vm_pop_object(vm, compiler, cz);
                ASSERT(object_l.var.is_reference == false);
                ASSERT(object_l.var.type.tag == data_type_Basic); // TODO:
                ASSERT(object_l.var.type.index_for_tag == data_basic_Int);

                dck_stretchy_push(vm->code, vm_inst_StoreInt);
                dck_stretchy_push(vm->code, 1);

                /* fallthrough */
            case abs_inst_JmpZe: /* fallthrough */
            case abs_inst_JmpNz:
                object_r = vm_pop_object(vm, compiler, cz);
                ASSERT(object_r.var.is_reference == false);
                ASSERT(object_r.var.type.tag == data_type_Basic); // TODO:
                ASSERT(object_r.var.type.index_for_tag == data_basic_Int);

                dck_stretchy_push(vm->code, vm_inst_StoreInt);
                dck_stretchy_push(vm->code, 0);

                /* fallthrough */
            case abs_inst_JmpUc: {
                vm_inst_t jmp_inst = vm_inst_JmpUc + (code.inst - abs_inst_JmpUc);
                dck_stretchy_push(vm->code, jmp_inst);

                ASSERT(inst_index + 1 < func->code_count);
                u32 label_index = cz->abs_code.data[func->code_offset + ++inst_index].index;

                u32 i = label_offset;
                for (; i < compiler->labels.count; ++i) {
                    vm_patch_t patch = compiler->labels.data[i];

                    if (patch.label_index == label_index) {
                        i32 rel_offset = (i32)patch.absolute_offset - (i32)(vm->code.count + 1);
                        dck_stretchy_push(vm->code, *((u32*)(&rel_offset)));
                        break;
                    }
                }

                if (i == compiler->labels.count) {
                    dck_stretchy_push(compiler->jump_patches, (vm_patch_t) {
                        .label_index     = label_index,
                        .absolute_offset = vm->code.count,
                    });
                    dck_stretchy_push(vm->code, 0xDEADC0DE);
                }
            } continue;

            case ABS_INST_COUNT: UNREACHABLE();
        }

        fprintf(stderr, "compiler: 'Encountered unknown meta instruction: %d'\n", code.inst);
        exit(1);
    }

    if (eval_offset != compiler->objects.count) {
        // TODO: Copy the base pointer and return address to the top of the stack.

        u32 result_object_count = compiler->objects.count - eval_offset;
        vm_object_t object = compiler->objects.data[eval_offset];

        u32 move_dst  = 0;
        u32 move_src  = object.base_offset;
        u32 move_size = object.size;

        for (u32 i = 1; i < result_object_count; ++i) {
            if (move_dst == move_src)
                break;

            object = compiler->objects.data[eval_offset + i];

            u32 pos = move_dst + move_size;
            u32 pos_aligned = vm_align(pos, object.alignment);
            u32 diff = pos_aligned - pos;

            if (diff == object.base_offset - object.prev_mem_off) {
                move_size += diff + object.size;
                continue;
            }

            dck_stretchy_push(vm->code, vm_inst_MemMove);
            dck_stretchy_push(vm->code, move_dst);
            dck_stretchy_push(vm->code, move_src);
            dck_stretchy_push(vm->code, move_size);

            move_dst  = pos_aligned;
            move_src  = object.base_offset;
            move_size = object.base_offset;
        }

        if (move_dst != move_src) {
            dck_stretchy_push(vm->code, vm_inst_MemMove);
            dck_stretchy_push(vm->code, move_dst);
            dck_stretchy_push(vm->code, move_src);
            dck_stretchy_push(vm->code, move_size);
        }
    }
    else {
        // TODO 
    }

    dck_stretchy_push(vm->code, vm_inst_Halt);

    vm_func_t function = {
        .func_ref      = func_ref,
        .code_offset   = code_offset,
        .object_offset = compiler->func_objects.count,
        .in_count      = variable_offset - input_offset,
        .var_count     = eval_offset - variable_offset,
        .out_count     = compiler->objects.count - eval_offset,
    };

    for (u32 i = input_offset; i < variable_offset; ++i) {
        dck_stretchy_push(compiler->func_objects, compiler->objects.data[i]);
    }

    for (u32 i = variable_offset; i < eval_offset; ++i) {
        dck_stretchy_push(compiler->func_objects, compiler->objects.data[i]);
    }

    for (u32 i = eval_offset; i < compiler->objects.count; ++i) {
        dck_stretchy_push(compiler->func_objects, compiler->objects.data[i]);
    }

    u32 function_index = compiler->funcs.count;

    dck_stretchy_push(compiler->funcs, function);

    return (vm_func_ref_t) {
        .func_index  = function_index,
        .code_offset = code_offset,
    };
}

void
vm_init(vm_t *vm, u32 code_offset)
{
    vm->ip = code_offset;
    vm->bp = 0;
    vm->ap = 0;

    vm->popped_pos = 0;
}

void
vm_clear(vm_t *vm)
{
    vm->memory.count = 0;
}

void
vm_step(vm_t *vm, cz_t *cz)
{
    (void)cz;

    ASSERT(vm->ip < vm->code.count);

    vm_inst_t inst = vm->code.data[vm->ip];

    if (inst == vm_inst_Halt)
        return;

    vm->ip++;

    vm_registers_t *r = &(vm->regs);

    switch (inst) {
        case vm_inst_IncSP: {
            ASSERT(vm->ip + 1 <= vm->code.count);
            i32 amount = *(i32 *)(vm->code.data + vm->ip++);

            if (amount > 0) {
                dck_stretchy_reserve(vm->memory, amount);
            }

            i32 new_count = (i32)vm->memory.count + amount;
            ASSERT(new_count >= 0);

            vm->memory.count = (u32)new_count;
        } break;

        case vm_inst_MemMove: {
            ASSERT(vm->ip + 3 <= vm->code.count);
            u32 dst  = vm->code.data[vm->ip++];
            u32 src  = vm->code.data[vm->ip++];
            u32 size = vm->code.data[vm->ip++];

            // 2 access checks
            u32 max_off = dst;
            if (max_off < src)
                max_off = src;
            if (max_off + size + vm->bp > vm->memory.count) {
                dck_stretchy_reserve(vm->memory, max_off + size + vm->bp - vm->memory.count);
            }

            memmove(vm->memory.data + vm->bp + dst, vm->memory.data + vm->bp + src, size);
        } break;

        case vm_inst_AddInt: r->integer[0] = r->integer[0] + r->integer[1]; break;
        case vm_inst_SubInt: r->integer[0] = r->integer[0] - r->integer[1]; break;

        case vm_inst_LoadInt: {
            ASSERT(vm->ip + 1 <= vm->code.count);
            u32 reg = vm->code.data[vm->ip++];

            u32 size = sizeof(i32);

            dck_stretchy_reserve(vm->memory, size);

            *(i32 *)(vm->memory.data + vm->memory.count) = vm->regs.integer[reg];
            vm->memory.count += size;
        } break;

        case vm_inst_StoreInt: {
            ASSERT(vm->ip + 1 <= vm->code.count);
            u32 reg = vm->code.data[vm->ip++];

            u32 size = sizeof(i32);
            ASSERT(vm->memory.count >= size);
            vm->memory.count -= size;

            vm->regs.integer[reg] = *(i32 *)(vm->memory.data + vm->memory.count);
        } break;

        case vm_inst_Load: {
            ASSERT(vm->ip + 2 <= vm->code.count);
            u32 base_offset = vm->code.data[vm->ip++];
            u32 size        = vm->code.data[vm->ip++];

            dck_stretchy_reserve(vm->memory, size);

            u32 abs_offset  = vm->bp + base_offset;
            memcpy(vm->memory.data + vm->memory.count, vm->memory.data + abs_offset, size);
            vm->memory.count += size;
        } break;

        case vm_inst_Store: {
            ASSERT(vm->ip + 2 <= vm->code.count);
            u32 base_offset = vm->code.data[vm->ip++];
            u32 size        = vm->code.data[vm->ip++];

            ASSERT(vm->memory.count >= size);
            vm->memory.count -= size;

            u32 abs_offset  = vm->bp + base_offset;
            ASSERT(vm->memory.count >= base_offset + size);

            memcpy(vm->memory.data + abs_offset, vm->memory.data + vm->memory.count, size);
        } break;

        case vm_inst_LoadImm: {
            ASSERT(vm->ip + 1 <= vm->code.count);
            u32 size = vm->code.data[vm->ip++];

            dck_stretchy_reserve(vm->memory, size);

            u32 rounded_size = (size + sizeof(vm_inst_t) - 1) / sizeof(vm_inst_t);
            ASSERT(vm->ip + rounded_size <= vm->code.count);

            memcpy(vm->memory.data + vm->memory.count, vm->code.data + vm->ip, size);
            vm->memory.count += size;
            vm->ip += rounded_size;
        } break;

        case vm_inst_RelToAbs: {
            ASSERT(vm->ip + 1 <= vm->code.count);
            u32 base_offset = vm->code.data[vm->ip++];

            u32 abs_offset  = vm->bp + base_offset;

            dck_stretchy_reserve(vm->memory, sizeof(abs_offset));

            *(u32 *)(vm->memory.data + vm->memory.count) = abs_offset;
            vm->memory.count += sizeof(abs_offset);
        } break;

        case vm_inst_StoreAP: {
            ASSERT(vm->memory.count >= sizeof(u32));
            vm->memory.count -= sizeof(u32);
            vm->ap = *(u32 *)(vm->memory.data + vm->memory.count);
        } break;

        case vm_inst_LoadAP: {
            dck_stretchy_reserve(vm->memory, sizeof(u32));
            *(u32 *)(vm->memory.data + vm->memory.count) = vm->ap;
            vm->memory.count += sizeof(u32);
        } break;

        case vm_inst_IncMulAP: {
            ASSERT(vm->ip + 1 <= vm->code.count);
            i32 multiple = (i32)(vm->code.data[vm->ip++]);

            ASSERT(vm->memory.count >= sizeof(i32));
            vm->memory.count -= sizeof(i32);
            i32 index = *(i32 *)(vm->memory.data + vm->memory.count);

            vm->ap = (u32)((i32)vm->ap + index * multiple);
        } break;

        case vm_inst_LoadAbs: {
            ASSERT(vm->ip + 1 <= vm->code.count);
            u32 size = vm->code.data[vm->ip++];

            dck_stretchy_reserve(vm->memory, size);

            memcpy(vm->memory.data + vm->memory.count, vm->memory.data + vm->ap, size);
            vm->memory.count += size;
        } break;

        case vm_inst_StoreAbs: {
            ASSERT(vm->ip + 1 <= vm->code.count);
            u32 size = vm->code.data[vm->ip++];

            ASSERT(vm->memory.count >= size);
            vm->memory.count -= size;

            ASSERT(vm->memory.count >= vm->ap + size);

            memcpy(vm->memory.data + vm->ap, vm->memory.data + vm->memory.count, size);
        } break;

        case vm_inst_JmpUc: {
            ASSERT(vm->ip + 1 <= vm->code.count);
            i32 offset = *(i32 *)(vm->code.data + vm->ip++);

            vm->ip = (u32)(*(i32 *)(&vm->ip) + offset);
        } break;

        case vm_inst_JmpIntGe: /* fallthrough */
        case vm_inst_JmpIntLe: /* fallthrough */
        case vm_inst_JmpIntGt: /* fallthrough */
        case vm_inst_JmpIntLt: /* fallthrough */
        case vm_inst_JmpIntNe: /* fallthrough */
        case vm_inst_JmpIntEq: /* fallthrough */
        case vm_inst_JmpIntZe: /* fallthrough */
        case vm_inst_JmpIntNz: {
            ASSERT(vm->ip + 1 <= vm->code.count);
            i32 offset = *(i32 *)(vm->code.data + vm->ip++);

            b32 do_jump;
            switch (inst) {
                case vm_inst_JmpIntGe: do_jump = r->integer[0] >= r->integer[1]; break;
                case vm_inst_JmpIntLe: do_jump = r->integer[0] <= r->integer[1]; break;
                case vm_inst_JmpIntGt: do_jump = r->integer[0] >  r->integer[1]; break;
                case vm_inst_JmpIntLt: do_jump = r->integer[0] <  r->integer[1]; break;
                case vm_inst_JmpIntNe: do_jump = r->integer[0] != r->integer[1]; break;
                case vm_inst_JmpIntEq: do_jump = r->integer[0] == r->integer[1]; break;
                case vm_inst_JmpIntZe: do_jump = r->integer[0] == 0;             break;
                case vm_inst_JmpIntNz: do_jump = r->integer[0] != 0;             break;
                default: UNREACHABLE();
            }

            if (do_jump) {
                vm->ip = (u32)(*(i32 *)(&vm->ip) + offset);
            }
        } break;

        default:
            printf("vm: executing unknown instruction: %d\n", inst);
            exit(1);
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

void
vm_push_data(vm_t *vm, u32 alignment, u32 size, void *ptr)
{
    u8 *data = ptr;
    u32 to_reserve = size;
    u32 offset = 0;

    u32 remainder = vm->memory.count % alignment;
    if (remainder != 0) {
        offset = alignment - remainder;
        to_reserve += offset;
    }

    dck_stretchy_reserve(vm->memory, to_reserve);
    memcpy(vm->memory.data + vm->memory.count + offset, data, size);
    vm->memory.count += to_reserve;
}

void *
vm_get_data(vm_t *vm, u32 alignment, u32 size)
{
    u32 to_increment = size;
    u32 offset = 0;

    u32 remainder = vm->popped_pos % alignment;
    if (remainder != 0) {
        offset = alignment - remainder;
        to_increment += offset;
    }

    ASSERT(vm->popped_pos + to_increment < vm->memory.count);

    void *ptr = vm->memory.data + vm->popped_pos + offset;
    vm->popped_pos += to_increment;
    return ptr;
}

void *
vm_get_arr_data(vm_t *vm, u32 alignment, u32 size, u32 count)
{
    u32 to_increment = size * count;
    u32 offset = 0;

    u32 remainder = vm->popped_pos % alignment;
    if (remainder != 0) {
        offset = alignment - remainder;
        to_increment += offset;
    }

    ASSERT(vm->popped_pos + to_increment < vm->memory.count);

    void *ptr = vm->memory.data + vm->popped_pos + offset;
    vm->popped_pos += to_increment;
    return ptr;
}

b32
vm_print_instruction(vm_t *vm, cz_t *cz)
{
    (void)cz;

    ASSERT(vm->ip < vm->code.count);

    vm_inst_t inst = vm->code.data[vm->ip];

    printf("%03d  ", vm->ip);

    if (inst == vm_inst_Halt) {
        printf("halt\n");
        return false;
    }

    vm->ip++;

    switch (inst) {
        case vm_inst_IncSP: {
            ASSERT(vm->ip + 1 <= vm->code.count);
            u32 amount = vm->code.data[vm->ip++];

            printf("inc.sp %d\n", amount);
        } break;

        case vm_inst_MemMove: {
            ASSERT(vm->ip + 3 <= vm->code.count);
            u32 dst  = vm->code.data[vm->ip++];
            u32 src  = vm->code.data[vm->ip++];
            u32 size = vm->code.data[vm->ip++];

            printf("mem.move %d %d %d\n", dst, src, size);
        } break;

        case vm_inst_AddInt: {
            printf("add.int\n");
        } break;

        case vm_inst_LoadInt: {
            ASSERT(vm->ip + 1 <= vm->code.count);

            u32 reg = vm->code.data[vm->ip++];

            printf("load.int %d\n", reg);
        } break;

        case vm_inst_StoreInt: {
            ASSERT(vm->ip + 1 <= vm->code.count);

            u32 reg = vm->code.data[vm->ip++];

            printf("store.int %d\n", reg);
        } break;

        case vm_inst_Load: {
            ASSERT(vm->ip + 2 <= vm->code.count);

            u32 base_offset = vm->code.data[vm->ip++];
            u32 size        = vm->code.data[vm->ip++];

            printf("load %d %d\n", base_offset, size);
        } break;

        case vm_inst_Store: {
            ASSERT(vm->ip + 2 <= vm->code.count);

            u32 base_offset = vm->code.data[vm->ip++];
            u32 size        = vm->code.data[vm->ip++];

            printf("store %d %d\n", base_offset, size);
        } break;

        case vm_inst_LoadImm: {
            ASSERT(vm->ip + 1 <= vm->code.count);

            u32 size = vm->code.data[vm->ip++];

            printf("load.imm %d ...\n", size);

            u32 rounded_size = (size + sizeof(vm_inst_t) - 1) / sizeof(vm_inst_t);
            vm->ip += rounded_size;
        } break;

        case vm_inst_LoadAbs: {
            ASSERT(vm->ip + 1 <= vm->code.count);

            u32 size = vm->code.data[vm->ip++];

            printf("load.abs %d\n", size);
        } break;

        case vm_inst_StoreAbs: {
            ASSERT(vm->ip + 1 <= vm->code.count);

            u32 size = vm->code.data[vm->ip++];

            printf("store.abs %d\n", size);
        } break;

        case vm_inst_RelToAbs: {
            ASSERT(vm->ip + 1 <= vm->code.count);

            u32 base_offset = vm->code.data[vm->ip++];

            printf("rel_to_abs %d\n", base_offset);
        } break;

        case vm_inst_StoreAP: {
            printf("store.ap\n");
        } break;

        case vm_inst_IncMulAP: {
            ASSERT(vm->ip + 1 <= vm->code.count);

            u32 size = vm->code.data[vm->ip++];

            printf("inc_mul.ap %d\n", size);
        } break;

        case vm_inst_LoadAP: {
            printf("load.ap\n");
        } break;

        case vm_inst_JmpUc: {
            ASSERT(vm->ip + 1 <= vm->code.count);
            i32 offset = *((i32 *)(vm->code.data + vm->ip++));

            printf("jmp.uc %d\n", offset);
        } break;

        case vm_inst_JmpIntNz: /* fallthrough */
        case vm_inst_JmpIntZe: /* fallthrough */
        case vm_inst_JmpIntEq: /* fallthrough */
        case vm_inst_JmpIntNe: /* fallthrough */
        case vm_inst_JmpIntLt: /* fallthrough */
        case vm_inst_JmpIntGt: /* fallthrough */
        case vm_inst_JmpIntLe: /* fallthrough */
        case vm_inst_JmpIntGe: {
            ASSERT(vm->ip + 1 <= vm->code.count);
            i32 offset = *((i32 *)(vm->code.data + vm->ip++));

            char *suffs[] = { "nz", "ze", "eq", "ne", "lt", "gt", "le", "ge" };
            printf("jmp.int.%s %d\n", suffs[inst - vm_inst_JmpIntNz], offset);
        } break;

        default:
            printf("unknown instruction: %d\n", inst);
    }

    return true;
}

void
vm_disassemble(vm_t *vm, cz_t *cz, u32 code_offset)
{
    vm_init(vm, code_offset);

    while (vm_print_instruction(vm, cz))
        ;;
}
