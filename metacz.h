#ifndef METACZ_H_
#define METACZ_H_

#include "core/utils.h"
#include "core/dck.h"

#include <stdio.h>

typedef enum
{
    data_type_Basic,
    data_type_Struct,
    data_type_Array,

    DATA_TYPE_TAG_COUNT
} data_type_tag_t;

typedef struct
{
    data_type_tag_t tag;
    u32 index_for_tag;
} type_ref_t;

typedef enum
{
    data_basic_Int,

    DATA_BASIC_COUNT
} data_basic_t;

static inline const char *
basic_name(data_basic_t basic)
{
    switch (basic) {
        case data_basic_Int: return "int";

        case DATA_BASIC_COUNT: UNREACHABLE();
    }

    UNREACHABLE();
}

#define CZ_BASIC_TYPE(m_label) ((type_ref_t) { .tag = data_type_Basic, .index_for_tag = data_basic_##m_label })

typedef struct
{
    type_ref_t data_type;
    u32 struct_offset;
} data_struct_ent_t;

typedef struct
{
    u32 alignment;
    u32 size;
    u32 tile_size;
    u32 entry_offset;
    u32 entry_count;
} data_struct_t;

typedef struct
{
    type_ref_t type;
    u32 length;
} data_array_t;

typedef enum
{
    jmp_Uc = 0,
    jmp_Nz,
    jmp_Ze,
    jmp_Eq,
    jmp_Ne,
    jmp_Lt,
    jmp_Gt,
    jmp_Le,
    jmp_Ge,

    JMP_TYPE_COUNT
} jmp_type_t;

typedef enum
{
    abs_inst_Add,
    abs_inst_Sub,

    abs_inst_LoadIn,
    abs_inst_LoadVar,
    abs_inst_LoadImm,
    abs_inst_LoadGlobal,

    abs_inst_StoreIn,
    abs_inst_StoreVar,
    abs_inst_StoreImm,
    abs_inst_StoreGlobal,

    abs_inst_Call,
    abs_inst_Ret,

    abs_inst_JmpUc,
    abs_inst_JmpNz,
    abs_inst_JmpZe,
    abs_inst_JmpEq,
    abs_inst_JmpNe,
    abs_inst_JmpLt,
    abs_inst_JmpGt,
    abs_inst_JmpLe,
    abs_inst_JmpGe,

    ABS_INST_COUNT
} abs_inst_t;

typedef enum
{
    abs_ref_Var,
    abs_ref_In,
    abs_ref_Global,

    ABS_INST_REF_COUNT
} abs_ref_tag_t;

typedef struct
{
    abs_ref_tag_t tag;
    u32 index_for_tag;
} ref_t;

typedef union
{
    abs_inst_t inst;
    i32 value;
    u32 index;
} abs_code_t;

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

    u32 abs_func_index;
    u32 parent_func_index;
} rec_func_t;

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

    u32 in_base;
    u32 var_base;

    u32 parent_func_index;
} abs_func_t;

typedef struct
{
    u32 frame_offset;
    u32 frame_count;

    u32 stack_bottom;

    u32 patch_offset;

    b32 is_set;
    u32 stack_diff;
} scope_t;

typedef struct
{
    b32 is_linked;
    u32 rec_code_offset;
} scope_frame_t;

typedef struct
{
    u32 scope_index;
    u32 frame_index;
} frame_ref_t;

typedef struct
{
    u32 scope_frame_index;
    u32 rec_code_offset;
} scope_patch_t;

typedef struct
{
    u32 scope_index;
} scope_ref_t;

typedef struct
{
    u32 func_index;
} func_ref_t;

#define CZ_NO_ID 0xFFFFFFFF

typedef struct
{
    u8 *data;
    union {
        u64 count;
        u64 size;
    };
    u64 capacity;
} arena_t;

typedef struct
{
    type_ref_t type;
    u64 data_offset;
    u64 data_size;
} immediate_t;

typedef struct
{
    dck_stretchy_t (data_struct_ent_t, u32) data_struct_ents;
    dck_stretchy_t (data_struct_t,     u32) data_structs;
    dck_stretchy_t (data_array_t,      u32) data_arrays;

    arena_t imm_data;
    dck_stretchy_t (immediate_t, u32) immediates;

    dck_stretchy_t (abs_code_t, u32) abs_code;
    dck_stretchy_t (type_ref_t, u32) abs_func_ins;
    dck_stretchy_t (type_ref_t, u32) abs_func_outs;
    dck_stretchy_t (type_ref_t, u32) abs_func_vars;
    dck_stretchy_t (abs_func_t, u32) abs_funcs;

    dck_stretchy_t (abs_code_t, u32) rec_code;
    dck_stretchy_t (type_ref_t, u32) rec_func_ins;
    dck_stretchy_t (type_ref_t, u32) rec_func_outs;
    dck_stretchy_t (type_ref_t, u32) rec_func_vars;
    dck_stretchy_t (rec_func_t, u32) rec_funcs;

    dck_stretchy_t (scope_t,       u32) scopes;
    dck_stretchy_t (scope_frame_t, u32) scope_frames;
    dck_stretchy_t (scope_patch_t, u32) scope_patches;

    u32 type_stack_size;

    const char *error;
} cz_t;


u64
arena_alloc(arena_t *arena, u64 size, u64 alignment);


#define CZ_ERROR_CHECK(m_cz) \
do { \
    if (cz->error) { \
        fprintf(stderr, \
            "%s:%d ERROR:\n" \
            "    %s.\n" \
            , \
            __FILE__, __LINE__, \
            cz->error \
        ); \
        exit(1); \
    } \
} while (0)

void
type_printf(cz_t *cz, type_ref_t type, u32 depth);

#define CZ_ADD() \
do { \
    cz_code_add(cz); \
    CZ_ERROR_CHECK(cz); \
} while(0)
void
cz_code_add(cz_t *cz);

#define CZ_SUB() \
do { \
    cz_code_sub(cz); \
    CZ_ERROR_CHECK(cz); \
} while(0)
void
cz_code_sub(cz_t *cz);

#define CZ_LOAD_IMM(m_imm) \
do { \
    cz_code_load_imm(cz, m_imm); \
    CZ_ERROR_CHECK(cz); \
} while(0)
void
cz_code_load_imm(cz_t *cz, i32 imm);

#define CZ_LOAD(m_ref) \
do { \
    cz_code_load(cz, m_ref); \
    CZ_ERROR_CHECK(cz); \
} while(0)
void
cz_code_load(cz_t *cz, ref_t ref);

#define CZ_STORE(m_ref) \
do { \
    cz_code_store(cz, m_ref); \
    CZ_ERROR_CHECK(cz); \
} while(0)
void
cz_code_store(cz_t *cz, ref_t ref);


func_ref_t
cz_func_begin(cz_t *cz);

ref_t
cz_func_in(cz_t *cz, type_ref_t data_type);

void
cz_func_out(cz_t *cz, type_ref_t data_type);

ref_t
cz_func_var(cz_t *cz, type_ref_t data_type);

func_ref_t
cz_func_end(cz_t *cz);


scope_ref_t
cz_scope_begin(cz_t *cz);

frame_ref_t
cz_scope_frame(cz_t *cz);

#define CZ_LINK(m_frame_ref) \
do { \
    cz_scope_frame_link(cz, m_frame_ref); \
    CZ_ERROR_CHECK(cz); \
} while(0)
void
cz_scope_frame_link(cz_t *cz, frame_ref_t frame_ref);

#define CZ_END() \
do { \
    cz_scope_end(cz); \
    CZ_ERROR_CHECK(cz); \
} while(0)
void
cz_scope_end(cz_t *cz);


#define CZ_JMP(m_type, m_frame_ref) \
do { \
    cz_jmp_frame(cz, jmp_##m_type, m_frame_ref); \
    CZ_ERROR_CHECK(cz); \
} while(0)
void
cz_jmp_frame(cz_t *cz, jmp_type_t type, frame_ref_t frame);

#define CZ_JMP_END(m_type, m_frame_ref) \
do { \
    cz_jmp_end(cz, jmp_##m_type, m_frame_ref); \
    CZ_ERROR_CHECK(cz); \
} while(0)
void
cz_jmp_end(cz_t *cz, jmp_type_t type, scope_ref_t scope);


void
cz_debug_dump(cz_t *cz);

#endif // METACZ_H_
