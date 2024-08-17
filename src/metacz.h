#ifndef METACZ_H_
#define METACZ_H_

#include "core/utils.h"
#include "core/dck.h"

#include <stdio.h>

typedef enum
{
    data_type_Basic,
    data_type_Array,
    data_type_Struct,

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

#define CZ_BASIC_VAL(m_label) ((variable_t) { .type = { .tag = data_type_Basic, .index_for_tag = data_basic_##m_label } })
#define CZ_BASIC_REF(m_label) ((variable_t) { .type = { .tag = data_type_Basic, .index_for_tag = data_basic_##m_label }, .is_reference = true })

#define CZ_VAL(m_type) ((variable_t) { .type = (m_type) })
#define CZ_REF(m_type) ((variable_t) { .type = (m_type), .is_reference = true })

typedef struct
{
    type_ref_t type;
    u32 length;
} type_array_t;

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

#define ABS_INST_TABLE \
    O(Add)           /* (a, b) -> (c) */ \
    O(Sub)           /* (a, b) -> (c) */ \
    \
    O(LoadIn)        /* () -> (a) */ \
    O(LoadVar)       /* () -> (a) */ \
    O(LoadImm)       /* () -> (a) */ \
    O(LoadGlobal)    /* () -> (a) */ \
    \
    O(StoreIn)       /* (a) -> () */ \
    O(StoreVar)      /* (a) -> () */ \
    O(StoreImm)      /* (a) -> () */ \
    O(StoreGlobal)   /* (a) -> () */ \
    \
    O(LoadRefIn)     /* () -> (&a) */ \
    O(LoadRefVar)    /* () -> (&a) */ \
    O(LoadRefGlobal) /* () -> (&a) */ \
    \
    O(StoreRef)      /* (a, &a) -> () */ \
    \
    O(ArrRead)       /* ([int], [&arr<a>]) -> (&a) */ \
    O(ArrLength)     /* ([&arr<a>])        -> ([int]) */ \
    \
    O(Deref)         /* (&a) -> (a) */ \
    \
    O(Call)          /* ... -> ... */ \
    O(Ret)           /* () -> () */ \
    \
    O(ScopeBegin)    /* () -> () */ \
    O(ScopeEnd)      /* () -> () */ \
    O(Label)         /* () -> () */ \
    \
    O(JmpUc)         /* ()     -> () */ \
    O(JmpNz)         /* (a)    -> () */ \
    O(JmpZe)         /* (a)    -> () */ \
    O(JmpEq)         /* (a, b) -> () */ \
    O(JmpNe)         /* (a, b) -> () */ \
    O(JmpLt)         /* (a, b) -> () */ \
    O(JmpGt)         /* (a, b) -> () */ \
    O(JmpLe)         /* (a, b) -> () */ \
    O(JmpGe)         /* (a, b) -> () */ \
/**/

typedef enum //              stack:
{
#define O(m_name) abs_inst_##m_name,
    ABS_INST_TABLE
#undef O
    ABS_INST_COUNT
} abs_inst_t;

static inline const char *
abs_inst_name(abs_inst_t inst)
{
    switch (inst) {
#define O(m_name) case abs_inst_##m_name: return #m_name; 
    ABS_INST_TABLE
#undef O
        default:
            return "<unknown>";
    }
}

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

    u32 next_label_index;
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
    u32 label_index;
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
    type_ref_t type;
    b32 is_reference;
} variable_t;

typedef struct
{
    arena_t imm_data;
    dck_stretchy_t (immediate_t, u32) immediates;

    dck_stretchy_t (type_array_t, u32) array_types;

    dck_stretchy_t (abs_code_t, u32) abs_code;
    dck_stretchy_t (variable_t, u32) abs_func_ins;
    dck_stretchy_t (variable_t, u32) abs_func_outs;
    dck_stretchy_t (variable_t, u32) abs_func_vars;
    dck_stretchy_t (abs_func_t, u32) abs_funcs;

    dck_stretchy_t (abs_code_t, u32) rec_code;
    dck_stretchy_t (variable_t, u32) rec_func_ins;
    dck_stretchy_t (variable_t, u32) rec_func_outs;
    dck_stretchy_t (variable_t, u32) rec_func_vars;
    dck_stretchy_t (rec_func_t, u32) rec_funcs;

    dck_stretchy_t (scope_t,       u32) scopes;
    dck_stretchy_t (scope_frame_t, u32) scope_frames;

    u32 type_stack_size;

    const char *error;
} cz_t;


u64
arena_alloc(arena_t *arena, u64 size, u64 alignment);

static inline void
cz_noop(void) { }

#define CZ_ERROR_CHECK(m_cz) ( \
    ((m_cz)->error) ? ( \
        fprintf(stderr, \
            "%s:%d ERROR:\n" \
            "    %s.\n" \
            , \
            __FILE__, __LINE__, \
            (m_cz)->error \
        ), \
        exit(1) \
    ) \
    : ( cz_noop() ) \
) 

void
type_printf(cz_t *cz, type_ref_t type, u32 depth);

void
cz_code_call_func(cz_t *cz, func_ref_t func_ref);
#define CZ_CALL(m_func_ref) \
do { \
    cz_code_call_func(cz, m_func_ref); \
    CZ_ERROR_CHECK(cz); \
} while (0)

type_ref_t
cz_make_type_array(cz_t *cz, type_ref_t type, u32 length);

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

#define CZ_READ() \
do { \
    cz_code_arr_read(cz); \
    CZ_ERROR_CHECK(cz); \
} while (0)
void
cz_code_arr_read(cz_t *cz);

#define CZ_LENGTH() \
do { \
    cz_code_arr_length(cz); \
    CZ_ERROR_CHECK(cz); \
} while (0)
void
cz_code_arr_length(cz_t *cz);

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

#define CZ_LOAD_REF(m_ref) \
do { \
    cz_code_load_ref(cz, m_ref); \
    CZ_ERROR_CHECK(cz); \
} while(0)
void
cz_code_load_ref(cz_t *cz, ref_t ref);

#define CZ_STORE_REF() \
do { \
    cz_code_store_ref(cz); \
    CZ_ERROR_CHECK(cz); \
} while(0)
void
cz_code_store_ref(cz_t *cz);

#define CZ_DEREF() \
do { \
    cz_code_deref(cz); \
    CZ_ERROR_CHECK(cz); \
} while(0)
void
cz_code_deref(cz_t *cz);


func_ref_t
cz_func_begin(cz_t *cz);

#define CZ_IN(m_var_name, m_type_ref) \
    ref_t m_var_name = cz_func_in(cz, (variable_t) { .type = m_type_ref }); \
    CZ_ERROR_CHECK(cz)
#define CZ_IN_REF(m_var_name, m_type_ref) \
    ref_t m_var_name = cz_func_in(cz, (variable_t) { .type = m_type_ref, \
                                                     .is_reference = true }); \
    CZ_ERROR_CHECK(cz)
ref_t
cz_func_in(cz_t *cz, variable_t var);

#define CZ_OUT(m_type_ref) \
    cz_func_out(cz, (variable_t) { .type = m_type_ref }); \
    CZ_ERROR_CHECK(cz)
#define CZ_OUT_REF(m_type_ref) \
    cz_func_out(cz, (variable_t) { .type = m_type_ref, \
                                   .is_reference = true }); \
    CZ_ERROR_CHECK(cz)
void
cz_func_out(cz_t *cz, variable_t var);

#define CZ_VAR(m_var_name, m_type_ref) \
    ref_t m_var_name = cz_func_var(cz, (variable_t) { .type = m_type_ref }); \
    CZ_ERROR_CHECK(cz)
#define CZ_VAR_REF(m_var_name, m_type_ref) \
    ref_t m_var_name = cz_func_var(cz, (variable_t) { .type = m_type_ref, \
                                                      .is_reference = true }); \
    CZ_ERROR_CHECK(cz)
ref_t
cz_func_var(cz_t *cz, variable_t var);

func_ref_t
cz_func_end(cz_t *cz);

#define CZ_FUNC(m_func_name) \
    func_ref_t m_func_name = cz_func_begin(cz); \
    for (int _flag_##__LINE__ = 1; _flag_##__LINE__; cz_func_end(cz), _flag_##__LINE__ = 0)


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
