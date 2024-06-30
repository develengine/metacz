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

static u32
vm_align(u32 value, u32 alignment)
{
    u32 remainder = value % alignment;
    if (remainder != 0) {
        value += alignment - remainder;
    }
    return value;
}

static vm_object_t
vm_push_type(vm_t *vm, vm_compiler_t *compiler, cz_t *cz, type_ref_t type_ref)
{
    u32 prev_sp = compiler->allocated_memory;

    vm_allocation_t allocation = vm_type_to_allocation(cz, type_ref);

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
    };

    dck_stretchy_push(compiler->objects, object);

    compiler->allocated_memory += allocation.size;

    return object;
}

static vm_object_t
vm_pop_object(vm_compiler_t *compiler, cz_t *cz)
{
    (void)cz;

    vm_object_t object = compiler->objects.data[--(compiler->objects.count)];
    compiler->allocated_memory -= object.size;
    return object;
}

u32
vm_compile(vm_t *vm, vm_compiler_t *compiler, cz_t *cz, func_ref_t func_ref)
{
    compiler->allocated_memory   = 0;
    compiler->objects.count      = 0;
    compiler->jump_patches.count = 0;
    compiler->labels.count       = 0;

    u32 code_offset = vm->code.count;

    abs_func_t *func = cz->abs_funcs.data + func_ref.func_index;

    u32 object_offset = compiler->objects.count;
    (void)object_offset; // TODO: Useful when working with nested functions (closures).

    u32 input_offset = compiler->objects.count;

    for (u32 i = 0; i < func->in_count; ++i) {
        type_ref_t type = cz->abs_func_ins.data[func->in_offset + i];
        vm_object_t object = vm_alloc_object(compiler, cz, type);
        dck_stretchy_push(compiler->objects, object);
    }

    // TODO: return address + base pointer + Closure base pointers.

    u32 variable_offset = compiler->objects.count;

    for (u32 i = 0; i < func->var_count; ++i) {
        type_ref_t type = cz->abs_func_vars.data[func->var_offset + i];
        vm_object_t object = vm_alloc_object(compiler, cz, type);
        dck_stretchy_push(compiler->objects, object);
    }

    u32 eval_offset = compiler->objects.count;

    for (u32 inst_index = 0; inst_index < func->code_count; ++inst_index) {
        abs_code_t code = cz->abs_code.data[func->code_offset + inst_index];

        vm_object_t object;
        vm_object_t object_r;
        vm_object_t object_l;

        switch (code.inst) {
            case abs_inst_Add: /* fallthrough */
            case abs_inst_Sub: {
                // NOTE: Types need to be tilable.

                ASSERT(eval_offset + 2 <= compiler->allocated_memory);
                
                object_r = vm_pop_object(compiler, cz);
                ASSERT(object_r.type_ref.tag == data_type_Basic); // TODO:
                ASSERT(object_r.type_ref.index_for_tag == data_basic_Int);

                object_l = vm_pop_object(compiler, cz);
                ASSERT(object_l.type_ref.tag == data_type_Basic); // TODO:
                ASSERT(object_l.type_ref.index_for_tag == data_basic_Int);

                if (code.inst == abs_inst_Add) {
                    dck_stretchy_push(vm->code, vm_inst_AddInt);
                }
                else {
                    dck_stretchy_push(vm->code, vm_inst_SubInt);
                }

                vm_push_type(vm, compiler, cz, object_l.type_ref);
            } break;

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

                vm_push_type(vm, compiler, cz, object.type_ref);

                dck_stretchy_push(vm->code, vm_inst_Load);
                dck_stretchy_push(vm->code, object.base_offset);
                dck_stretchy_push(vm->code, object.size);
            } break;

            case abs_inst_LoadImm: {
                ASSERT(inst_index + 1 < func->code_count);

                u32 imm_index = cz->abs_code.data[func->code_offset + ++inst_index].index;
                immediate_t immediate = cz->immediates.data[imm_index];

                object = vm_push_type(vm, compiler, cz, immediate.type);

                dck_stretchy_push(vm->code, vm_inst_LoadImm);
                dck_stretchy_push(vm->code, object.size);

                u32 imm_inst_size = (object.size + sizeof(vm_inst_t) - 1) / sizeof(vm_inst_t);
                dck_stretchy_reserve(vm->code, imm_inst_size);

                u8 *imm_ptr = (u8 *)(cz->imm_data.data + immediate.data_offset);

                memcpy(vm->code.data + vm->code.count, imm_ptr, object.size);

                vm->code.count += imm_inst_size;
            } break;

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

                vm_object_t stack_object = compiler->objects.data[--(compiler->objects.count)];

                dck_stretchy_push(vm->code, vm_inst_Store);
                dck_stretchy_push(vm->code, object.base_offset);
                dck_stretchy_push(vm->code, stack_object.size);
                compiler->allocated_memory -= stack_object.size;
            } break;

            case abs_inst_Label: {
                ASSERT(inst_index + 1 < func->code_count);
                u32 label_index = cz->abs_code.data[func->code_offset + ++inst_index].index;

                for (u32 i = 0; i < compiler->jump_patches.count;) {
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
            } break;

            case abs_inst_JmpGe: /* fallthrough */
            case abs_inst_JmpLe: /* fallthrough */
            case abs_inst_JmpGt: /* fallthrough */
            case abs_inst_JmpLt: /* fallthrough */
            case abs_inst_JmpNe: /* fallthrough */
            case abs_inst_JmpEq:
                object_l = vm_pop_object(compiler, cz);
                ASSERT(object_l.type_ref.tag == data_type_Basic); // TODO:
                ASSERT(object_l.type_ref.index_for_tag == data_basic_Int);
                /* fallthrough */
            case abs_inst_JmpZe: /* fallthrough */
            case abs_inst_JmpNz:
                object_r = vm_pop_object(compiler, cz);
                ASSERT(object_r.type_ref.tag == data_type_Basic); // TODO:
                ASSERT(object_r.type_ref.index_for_tag == data_basic_Int);
                /* fallthrough */
            case abs_inst_JmpUc: {
                vm_inst_t jmp_inst = vm_inst_JmpUc + (code.inst - abs_inst_JmpUc);
                dck_stretchy_push(vm->code, jmp_inst);

                ASSERT(inst_index + 1 < func->code_count);
                u32 label_index = cz->abs_code.data[func->code_offset + ++inst_index].index;

                u32 i = 0;
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
            } break;
        }
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

    return code_offset;
}

void
vm_init(vm_t *vm, u32 code_offset)
{
    vm->ip = code_offset;
    vm->bp = 0;

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

    switch (inst) {
        case vm_inst_IncSP: {
            ASSERT(vm->ip + 1 <= vm->code.count);
            u32 amount = vm->code.data[vm->ip++];

            dck_stretchy_reserve(vm->memory, amount);
            vm->memory.count += amount;
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
            if (max_off + size > vm->memory.count) {
                dck_stretchy_reserve(vm->memory, max_off + size - vm->memory.count);
            }

            memmove(vm->memory.data + vm->bp + dst, vm->memory.data + vm->bp + src, size);
        } break;

        case vm_inst_AddInt: /* fallthrough */
        case vm_inst_SubInt: {
            ASSERT(vm->memory.count >= sizeof(i32) * 2);
            vm->memory.count -= sizeof(i32);
            i32 a = *(i32 *)(vm->memory.data + vm->memory.count);
            vm->memory.count -= sizeof(i32);
            i32 b = *(i32 *)(vm->memory.data + vm->memory.count);

            i32 res;
            if      (inst == vm_inst_AddInt) res = a + b;
            else if (inst == vm_inst_SubInt) res = a - b;
            else UNREACHABLE();

            *(i32 *)(vm->memory.data + vm->memory.count) = res;
            vm->memory.count += sizeof(i32);
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

            i32 a, b;
            if (inst != vm_inst_JmpIntZe && inst != vm_inst_JmpIntNz) {
                ASSERT(vm->memory.count >= sizeof(i32));
                vm->memory.count -= sizeof(i32);
                b = *(i32 *)(vm->memory.data + vm->memory.count);
            }

            ASSERT(vm->memory.count >= sizeof(i32));
            vm->memory.count -= sizeof(i32);
            a = *(i32 *)(vm->memory.data + vm->memory.count);

            b32 do_jump;
            switch (inst) {
                case vm_inst_JmpIntGe: do_jump = a >= b; break;
                case vm_inst_JmpIntLe: do_jump = a <= b; break;
                case vm_inst_JmpIntGt: do_jump = a > b;  break;
                case vm_inst_JmpIntLt: do_jump = a < b;  break;
                case vm_inst_JmpIntNe: do_jump = a != b; break;
                case vm_inst_JmpIntEq: do_jump = a == b; break;
                case vm_inst_JmpIntZe: do_jump = a == 0; break;
                case vm_inst_JmpIntNz: do_jump = a != 0; break;
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
            ASSERT(vm->ip + 2 <= vm->code.count);
            printf("add.int\n");
        } break;

        case vm_inst_Load: {
            ASSERT(vm->ip + 2 <= vm->code.count);

            u32 base_offset = vm->code.data[vm->ip++];
            u32 size        = vm->code.data[vm->ip++];

            printf("load %d %d\n", base_offset, size);
        } break;

        case vm_inst_LoadImm: {
            ASSERT(vm->ip + 1 <= vm->code.count);

            u32 size = vm->code.data[vm->ip++];

            printf("load.imm %d ...\n", size);

            u32 rounded_size = (size + sizeof(vm_inst_t) - 1) / sizeof(vm_inst_t);
            vm->ip += rounded_size;
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
