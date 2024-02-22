#include "core/utils.h"
#include "core/dck.h"

#include <stdio.h>

typedef enum
{
    data_basic_Int,

    DATA_BASIC_COUNT
} data_basic_t;

typedef struct
{
    u32 data_type_index;
    u32 struct_offset;
} data_struct_ent_t;

typedef struct
{
    u32 alignment;
    u32 size;
    u32 entry_offset;
    u32 entry_count;
} data_struct_t;

typedef struct
{
    u32 child_type_offset;
    u32 child_type_count;
} data_tuple_t;

typedef struct
{
    u32 data_type_index;
    u32 length;
} data_array_t;

typedef enum
{
    data_type_Basic,
    data_type_Struct,
    data_type_Tuple,
    data_type_Array,

    DATA_TYPE_TAG_COUNT
} data_type_tag_t;

typedef struct
{
    data_type_tag_t tag;
    u32 index_for_tag;
} data_type_t;

typedef struct
{
    data_type_t type;
    u32 base_offset;
} variable_t;

typedef struct
{
    u32 object_index;
} local_t;

typedef struct
{
    u32 in_tuple_index;
    u32 out_tuple_index;

    u32 parent_function_index;
    u32 local_offset;
    u32 local_count;

    u32 code_offset;
    u32 local_size;
} function_t;

typedef enum
{
    object_Data,
    object_Function,

    OBJECT_TAG_COUNT
} object_tag_t;

typedef struct
{
    object_tag_t tag;
    u32 index_for_tag;
} object_t;

typedef struct
{
    u32 local_base;
    u32 param_base;
} type_scope_t;

typedef enum
{
    abs_inst_Add,
    abs_inst_Sub,
    abs_inst_LoadImm,
    abs_inst_Call,
    abs_inst_Ret,
    abs_inst_Load,
    abs_inst_Store,

    ABS_INST_COUNT
} abs_inst_t;

typedef struct
{
    u32 in_offset;
    u32 in_count;
    u32 out_offset;
    u32 out_count;
    u32 var_offset;
    u32 var_count;
    u32 code_offset;
    u32 code_count;

    u32 parent_func_index;
} abs_func_t;

typedef enum
{
    abs_obj_Data,
    abs_obj_Function,

    ABS_OBJ_TAG_COUNT
} abs_obj_tag_t;

typedef struct
{
    abs_obj_tag_t tag;
    u32 index_for_tag;
} abs_obj_t;

typedef struct
{
    data_type_t type;
    u32 base_offset;
} abs_var_t;

typedef enum
{
    inst_Add,
    inst_Sub,
    inst_LoadImm,
    inst_Call,
    inst_Ret,
    inst_Load,
    inst_Store,

    INST_COUNT
} inst_t;

typedef struct
{
    u32 param_base;
    u32 local_base;
    u32 eval_base;
} state_t;

#define CZ_NO_ID 0xFFFFFFFF

typedef enum
{
    func_rec_Input = 0,
    func_rec_Output,
    func_rec_Impl,

    FUNC_REC_STATE_COUNT
} func_rec_state_t;

typedef struct
{
    func_rec_state_t state;
} func_rec_t;

typedef struct
{
    dck_stretchy_t (data_struct_ent_t, u32) data_struct_ents;
    dck_stretchy_t (data_struct_t,     u32) data_structs;
    dck_stretchy_t (data_array_t,      u32) data_arrays;
    dck_stretchy_t (data_type_t,       u32) data_types;

    dck_stretchy_t (u32,        u32) abs_code;
    dck_stretchy_t (u32,        u32) abs_func_outs;
    dck_stretchy_t (u32,        u32) abs_func_ins;
    dck_stretchy_t (abs_var_t,  u32) abs_func_vars;
    dck_stretchy_t (abs_func_t, u32) abs_funcs;

    dck_stretchy_t (u32,        u32) rec_code;
    dck_stretchy_t (u32,        u32) rec_func_outs;
    dck_stretchy_t (u32,        u32) rec_func_ins;
    dck_stretchy_t (abs_var_t,  u32) rec_func_vars;
    dck_stretchy_t (abs_func_t, u32) rec_funcs;
} cz_t;

u32
cz_func_begin(cz_t *cz)
{
    u32 rec_index = cz->rec_funcs.count;

    dck_stretchy_push(cz->rec_funcs, (abs_func_t) {
        .in_offset   = cz->rec_func_ins.count,
        .out_offset  = cz->rec_func_outs.count,
        .var_offset  = cz->rec_func_vars.count,
        .code_offset = cz->rec_code.count,

        .parent_func_index = CZ_NO_ID,
    });

    return rec_index;
}

u32
cz_func_end(cz_t *cz)
{
    u32 abs_index = cz->abs_funcs.count;

    cz->rec_funcs.count--;

    abs_func_t *rec_func = cz->rec_funcs.data + cz->rec_funcs.count;

    dck_stretchy_push(cz->abs_funcs, (abs_func_t) {
        .in_offset   = cz->abs_func_ins.count,
        .in_count    = rec_func->in_count,
        .out_offset  = cz->abs_func_outs.count,
        .out_count   = rec_func->out_count,
        .var_offset  = cz->abs_func_vars.count,
        .var_count   = rec_func->var_count,
        .code_offset = cz->abs_code.count,
        .code_count  = rec_func->code_count,

        .parent_func_index = CZ_NO_ID,
    });

    abs_func_t *abs_func = cz->abs_funcs.data + abs_index;

    // TODO: Unshit this code.

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

    for (u32 i = 0; i < rec_func->code_count; ++i) {
        dck_stretchy_push(cz->abs_code, cz->rec_code.data[rec_func->code_offset + i]);
    }
    cz->rec_code.count = rec_func->code_offset;

    return abs_index;
}

u32
cz_struct_begin(cz_t *cz)
{
    return CZ_NO_ID;
}

u32
cz_struct_end(cz_t *cz)
{
    return CZ_NO_ID;
}

u32
cz_array_create(cz_t *cz, u32 type_index, u32 size)
{
    return CZ_NO_ID;
}


int
main(void)
{
    cz_t cz = {0};

    printf("\\_/\n V\n");
    return 0;
}
