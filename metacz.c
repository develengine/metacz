#include "metacz.h"

u64
arena_alloc(arena_t *arena, u64 size, u64 alignment)
{
    u64 remainder = arena->size % alignment;
    dck_stretchy_reserve(*arena, size + remainder);
    u64 base = arena->size + remainder;
    arena->size += size + remainder;
    return base;
}


void
cz_debug_dump(cz_t *cz)
{
    for (u32 fun_index = 0; fun_index < cz->abs_funcs.count; ++fun_index) {
        abs_func_t *func = cz->abs_funcs.data + fun_index;

        printf("%d: ", fun_index);

        for (u32 in_index = 0; in_index < func->in_count; ++in_index) {
            if (in_index != 0) {
                printf(", ");
            }

            type_ref_t type = cz->abs_func_ins.data[func->in_offset + in_index];

            if (type.tag == data_type_Basic) {
                printf("%s", basic_name(type.index_for_tag));
            }
            else {
                printf("%d.%d", type.tag, type.index_for_tag);
            }
        }

        printf(" -> ");

        for (u32 out_index = 0; out_index < func->out_count; ++out_index) {
            if (out_index != 0) {
                printf(", ");
            }

            type_ref_t type = cz->abs_func_outs.data[func->out_offset + out_index];

            if (type.tag == data_type_Basic) {
                printf("%s", basic_name(type.index_for_tag));
            }
            else {
                printf("%d.%d", type.tag, type.index_for_tag);
            }
        }

        printf("\n");

        for (i32 code_index = 0; code_index < func->code_count; ++code_index) {
            abs_code_t code = cz->abs_code.data[func->code_offset + code_index];
            switch (code.inst) {
                case abs_inst_Add:
                    printf("     add\n");
                    break;
                case abs_inst_Sub:
                    printf("     sub\n");
                    break;
                case abs_inst_LoadIn:
                    printf("     load in\n");
                    printf("       %d\n", cz->abs_code.data[func->code_offset + ++code_index].value);
                    break;
                case abs_inst_LoadVar:
                    printf("     load var\n");
                    printf("       %d\n", cz->abs_code.data[func->code_offset + ++code_index].value);
                    break;
                case abs_inst_LoadImm:
                    printf("     load imm\n");
                    printf("       %d\n", cz->abs_code.data[func->code_offset + ++code_index].value);
                    break;
                case abs_inst_LoadGlobal:
                    printf("     load global\n");
                    printf("       %d\n", cz->abs_code.data[func->code_offset + ++code_index].value);
                    break;
                case abs_inst_StoreIn:
                    printf("     store in\n");
                    printf("       %d\n", cz->abs_code.data[func->code_offset + ++code_index].value);
                    break;
                case abs_inst_StoreVar:
                    printf("     store var\n");
                    printf("       %d\n", cz->abs_code.data[func->code_offset + ++code_index].value);
                    break;
                case abs_inst_StoreImm:
                    printf("     store imm\n");
                    printf("       %d\n", cz->abs_code.data[func->code_offset + ++code_index].value);
                    break;
                case abs_inst_StoreGlobal:
                    printf("     store global\n");
                    printf("       %d\n", cz->abs_code.data[func->code_offset + ++code_index].value);
                    break;
                case abs_inst_Call:
                    break;
                case abs_inst_Ret:
                    break;
                case abs_inst_JmpUc: /* fallthrough */
                    printf("     jmp uc\n");
                case abs_inst_JmpNz: /* fallthrough */
                    printf("     jmp nz\n");
                case abs_inst_JmpZe: /* fallthrough */
                    printf("     jmp ze\n");
                case abs_inst_JmpEq: /* fallthrough */
                    printf("     jmp eq\n");
                case abs_inst_JmpNe: /* fallthrough */
                    printf("     jmp ne\n");
                case abs_inst_JmpLt: /* fallthrough */
                    printf("     jmp lt\n");
                case abs_inst_JmpGt: /* fallthrough */
                    printf("     jmp gt\n");
                case abs_inst_JmpLe: /* fallthrough */
                    printf("     jmp le\n");
                case abs_inst_JmpGe:
                    printf("     jmp ge\n");
                    printf("       %d\n", cz->abs_code.data[func->code_offset + ++code_index].value);
                    break;
            }
        }
    }
}

void
type_printf(cz_t *cz, type_ref_t type, u32 depth)
{
    for (u32 i = 0; i < depth; ++i) {
        printf("  ");
    }

    if (type.tag == data_type_Basic) {
        const char *name = basic_name(type.index_for_tag);
        printf("%s\n", name);
        return;
    }

    printf("<unhandled>\n");
}

type_ref_t
cz_ref_type(cz_t *cz, ref_t ref)
{
    switch (ref.tag) {
        case abs_ref_Var:    return cz->rec_func_vars.data[ref.index_for_tag];
        case abs_ref_In:     return cz->rec_func_ins.data [ref.index_for_tag];
        case abs_ref_Global: return cz->rec_func_outs.data[ref.index_for_tag];

        case ABS_INST_REF_COUNT: UNREACHABLE();
    }

    UNREACHABLE();
}

type_ref_t
cz_struct_begin(cz_t *cz)
{
    u32 struct_index = cz->data_structs.count;

    dck_stretchy_push(cz->data_structs, (data_struct_t) {
        .entry_offset = cz->data_struct_ents.count,
    });

    return (type_ref_t) {
        .tag           = data_type_Struct,
        .index_for_tag = struct_index,
    };
}

u32
cz_struct_ent(cz_t *cz, type_ref_t data_type)
{
    data_struct_t *structure = cz->data_structs.data + cz->data_structs.count - 1;

    dck_stretchy_push(cz->data_struct_ents, (data_struct_ent_t) {
        .data_type     = data_type,
        .struct_offset = 0, // TODO:
    });

    return structure->entry_count++;
}

void
cz_struct_end(cz_t *cz)
{
    (void)cz;
    // TODO: Finalize something?
}

u32
cz_array_create(cz_t *cz, type_ref_t data_type, u32 size)
{
    return CZ_NO_ID; // TODO:
}

func_ref_t
cz_func_begin(cz_t *cz)
{
    u32 abs_index = cz->abs_funcs.count;

    dck_stretchy_push(cz->abs_funcs, (abs_func_t) {0});

    u32 parent_index = CZ_NO_ID;

    if (cz->rec_funcs.count > 0) {
        parent_index = cz->rec_funcs.data[cz->rec_funcs.count - 1].abs_func_index;
    }

    dck_stretchy_push(cz->rec_funcs, (rec_func_t) {
        .in_offset   = cz->rec_func_ins.count,
        .out_offset  = cz->rec_func_outs.count,
        .var_offset  = cz->rec_func_vars.count,
        .code_offset = cz->rec_code.count,

        .abs_func_index    = abs_index,
        .parent_func_index = parent_index,
    });

    return (func_ref_t) { .func_index = abs_index };
}

ref_t
cz_func_in(cz_t *cz, type_ref_t data_type)
{
    cz->rec_funcs.data[cz->rec_funcs.count - 1].in_count++;
    u32 id = cz->rec_func_ins.count;
    dck_stretchy_push(cz->rec_func_ins, data_type);
    return (ref_t) { .tag = abs_ref_In, .index_for_tag = id };
}

void
cz_func_out(cz_t *cz, type_ref_t data_type)
{
    cz->rec_funcs.data[cz->rec_funcs.count - 1].out_count++;
    dck_stretchy_push(cz->rec_func_outs, data_type);
}

ref_t
cz_func_var(cz_t *cz, type_ref_t data_type)
{
    cz->rec_funcs.data[cz->rec_funcs.count - 1].var_count++;
    u32 id = cz->rec_func_vars.count;
    dck_stretchy_push(cz->rec_func_vars, data_type);
    return (ref_t) { .tag = abs_ref_Var, .index_for_tag = id };
}

void
cz_code_add(cz_t *cz)
{
    if (cz->type_stack_size < 2) {
        cz->error = "Addition of less than 2 items";
        return;
    }

    cz->type_stack_size--;

    dck_stretchy_push(cz->rec_code, (abs_code_t) { .inst = abs_inst_Add });
}

void
cz_code_sub(cz_t *cz)
{
    if (cz->type_stack_size < 2) {
        cz->error = "Subtraction of less than 2 items";
        return;
    }

    cz->type_stack_size--;

    dck_stretchy_push(cz->rec_code, (abs_code_t) { .inst = abs_inst_Sub });
}

void
cz_code_load_imm(cz_t *cz, i32 imm)
{
    cz->type_stack_size++;

    u64 data_offset = arena_alloc(&cz->imm_data, sizeof(i32), _Alignof(i32));

    i32 *imm_ptr = (i32 *)(cz->imm_data.data + data_offset);
    *imm_ptr = imm;

    u32 imm_index = cz->immediates.count;

    dck_stretchy_push(cz->immediates, (immediate_t) {
        .type = CZ_BASIC_TYPE(Int),
        .data_offset = data_offset,
        .data_size = sizeof(i32),
    });

    dck_stretchy_push(cz->rec_code, (abs_code_t) { .inst  = abs_inst_LoadImm });
    dck_stretchy_push(cz->rec_code, (abs_code_t) { .index = imm_index });
}

void
cz_code_load(cz_t *cz, ref_t ref)
{
    cz->type_stack_size++;

    abs_code_t code;

    switch (ref.tag) {
        case abs_ref_Var:    code.inst = abs_inst_LoadVar;    break;
        case abs_ref_In:     code.inst = abs_inst_LoadIn;     break;
        case abs_ref_Global: code.inst = abs_inst_LoadGlobal; break;

        default: UNREACHABLE();
    }

    dck_stretchy_push(cz->rec_code, code);
    dck_stretchy_push(cz->rec_code, (abs_code_t) { .value = ref.index_for_tag });
}

void
cz_code_store(cz_t *cz, ref_t ref)
{
    if (cz->type_stack_size < 1) {
        cz->error = "Store from empty stack";
        return;
    }

    abs_code_t code;

    switch (ref.tag) {
        case abs_ref_Var:    code.inst = abs_inst_StoreVar;    break;
        case abs_ref_In:     code.inst = abs_inst_StoreIn;     break;
        case abs_ref_Global: code.inst = abs_inst_StoreGlobal; break;

        default: UNREACHABLE();
    }

    dck_stretchy_push(cz->rec_code, code);
    dck_stretchy_push(cz->rec_code, (abs_code_t) { .value = ref.index_for_tag });
}


func_ref_t
cz_func_end(cz_t *cz)
{
    cz->rec_funcs.count--;

    rec_func_t *rec_func = cz->rec_funcs.data + cz->rec_funcs.count;

    if (cz->type_stack_size < rec_func->out_count) {
        cz->error = "Stack has less elements than function output";
        return (func_ref_t) { .func_index = CZ_NO_ID };
    }

    cz->type_stack_size -= rec_func->out_count;

    u32 abs_index = rec_func->abs_func_index;

    abs_func_t *abs_func = cz->abs_funcs.data + abs_index;

    u32 code_count = cz->rec_code.count - rec_func->code_offset;

    *abs_func = (abs_func_t) {
        .in_offset   = cz->abs_func_ins.count,
        .in_count    = rec_func->in_count,
        .out_offset  = cz->abs_func_outs.count,
        .out_count   = rec_func->out_count,
        .var_offset  = cz->abs_func_vars.count,
        .var_count   = rec_func->var_count,
        .code_offset = cz->abs_code.count,
        .code_count  = code_count,

        .in_base  = rec_func->in_offset,
        .var_base = rec_func->var_offset,

        .parent_func_index = rec_func->parent_func_index,
    };

    for (u32 i = 0; i < rec_func->in_count; ++i) {
        dck_stretchy_push(cz->abs_func_ins, cz->rec_func_ins.data[rec_func->in_offset + i]);
    }
    cz->rec_func_ins.count = rec_func->in_offset;

    for (u32 i = 0; i < rec_func->out_count; ++i) {
        dck_stretchy_push(cz->abs_func_outs, cz->rec_func_outs.data[rec_func->out_offset + i]);
    }
    cz->rec_func_outs.count = rec_func->out_offset;

    for (u32 i = 0; i < rec_func->var_count; ++i) {
        dck_stretchy_push(cz->abs_func_vars, cz->rec_func_vars.data[rec_func->var_offset + i]);
    }
    cz->rec_func_vars.count = rec_func->var_offset;

    for (u32 i = 0; i < code_count; ++i) {
        dck_stretchy_push(cz->abs_code, cz->rec_code.data[rec_func->code_offset + i]);
    }
    cz->rec_code.count = rec_func->code_offset;

    return (func_ref_t) { .func_index = abs_index };
}

static b32
cz_scope_check(cz_t *cz, u32 scope_index)
{
    scope_t *scope = cz->scopes.data + scope_index;

    u32 stack_diff = cz->type_stack_size - scope->stack_bottom;

    if (!scope->is_set) {
        scope->is_set = true;

        scope->stack_diff = stack_diff;

        return true;
    }

    if (stack_diff != scope->stack_diff) {
        cz->error = "Scope finishes with a non-compatible difference";
        return false;
    }

    return true;
}

scope_ref_t
cz_scope_begin(cz_t *cz)
{
    rec_func_t *func = cz->rec_funcs.data + cz->rec_funcs.count - 1;

    u32 end_frame_index = cz->scope_frames.count;

    dck_stretchy_push(cz->scope_frames, (scope_frame_t) {
        .label_index = func->next_label_index++,
    });

    u32 scope_index = cz->scopes.count;

    dck_stretchy_push(cz->scopes, (scope_t) {
        .frame_offset = end_frame_index,
        .frame_count  = 1,
        .stack_bottom = cz->type_stack_size,
    });

    return (scope_ref_t) {
        .scope_index = scope_index,
    };
}

frame_ref_t
cz_scope_frame(cz_t *cz)
{
    rec_func_t *func = cz->rec_funcs.data + cz->rec_funcs.count - 1;

    u32 scope_index = cz->scopes.count - 1;

    scope_t *scope = cz->scopes.data + scope_index;

    u32 child_index = scope->frame_count++;

    dck_stretchy_push(cz->scope_frames, (scope_frame_t) {
        .label_index = func->next_label_index++,
    });

    return (frame_ref_t) {
        .scope_index = scope_index,
        .frame_index = child_index,
    };
}

void
cz_scope_frame_link(cz_t *cz, frame_ref_t frame_ref)
{
    if (frame_ref.scope_index != cz->scopes.count - 1) {
        cz->error = "Linking of a frame from a different scope";
        return;
    }

    scope_t *scope = cz->scopes.data + frame_ref.scope_index;

    if (cz->type_stack_size != scope->stack_bottom) {
        cz->error = "Linking of a frame with excess data on the stack";
        return;
    }

    scope_frame_t *frame = cz->scope_frames.data + scope->frame_offset + frame_ref.frame_index;

    if (frame->is_linked) {
        cz->error = "Linking of an already linked frame";
        return;
    }

    frame->is_linked = true;

    dck_stretchy_push(cz->rec_code, (abs_code_t) { .inst = abs_inst_Label });
    dck_stretchy_push(cz->rec_code, (abs_code_t) { .value = frame->label_index });
}

void
cz_scope_end(cz_t *cz)
{
    if (cz->scopes.count == 0) {
        cz->error = "Ending of scope without a corresponding begin";
        return;
    }

    u32 scope_index = --(cz->scopes.count);

    scope_t *scope = cz->scopes.data + scope_index;

    if (scope->is_set) {
        if (!cz_scope_check(cz, scope_index))
            return;
    }

    cz->scope_frames.count = scope->frame_offset;
    cz->scopes.count = scope_index;
}

static b32
cz_jmp_check(cz_t *cz, jmp_type_t jmp_type)
{
    // FIXME: This should take the scope bottom into consideration.

    if (jmp_type == jmp_Uc)
        return true;

    if (jmp_type == jmp_Nz || jmp_type == jmp_Ze) {
        if (cz->type_stack_size < 1) {
            cz->error = "Zero based jump with empty stack";
            return false;
        }

        cz->type_stack_size--;

        return true;
    }

    if (cz->type_stack_size < 2) {
        cz->error = "Comparison based jump with less than 2 types on the stack";
        return false;
    }

    cz->type_stack_size -= 2;

    return true;
}

void
cz_jmp_frame(cz_t *cz, jmp_type_t type, frame_ref_t frame_ref)
{
    if (!cz_jmp_check(cz, type))
        return;

    scope_t *scope = cz->scopes.data + frame_ref.scope_index;

    if (cz->type_stack_size != scope->stack_bottom) {
        cz->error = "Jump to a frame with excess data on the stack";
        return;
    }

    scope_frame_t *frame = cz->scope_frames.data + scope->frame_offset + frame_ref.frame_index;

    dck_stretchy_push(cz->rec_code, (abs_code_t) { .inst = abs_inst_JmpUc + type });
    dck_stretchy_push(cz->rec_code, (abs_code_t) { .value = frame->label_index });
}

void
cz_jmp_end(cz_t *cz, jmp_type_t type, scope_ref_t scope_ref)
{
    // FIXME: This must be retarded wrong.

    if (!cz_jmp_check(cz, type))
        return;

    u32 scope_index = cz->scopes.count - 1;

    if (!cz_scope_check(cz, scope_index))
        return;

    scope_t *scope = cz->scopes.data + scope_index;

    if (type == jmp_Uc) {
        cz->type_stack_size = scope->stack_bottom;
    }

    scope_frame_t *frame = cz->scope_frames.data + scope->frame_offset;

    dck_stretchy_push(cz->rec_code, (abs_code_t) { .inst = abs_inst_JmpUc + type });
    dck_stretchy_push(cz->rec_code, (abs_code_t) { .value = frame->label_index });
}

