#ifndef INTERPRETER_H_
#define INTERPRETER_H_

#include "metacz.h"

/*
 * Interpreter
 */

#define VM_INST_TABLE \
    O(Halt) \
    \
    O(IncSP) \
    O(MemMove) \
    \
    O(AddInt) \
    O(SubInt) \
    \
    O(Load) \
    O(LoadImm) \
    O(LoadAbs) \
    O(LoadAP) \
    O(LoadBP) \
    O(LoadIP) \
    \
    O(LoadInt) \
    O(LoadAddr) \
    \
    O(Store) \
    O(StoreAbs) \
    O(StoreAP) \
    \
    O(StoreInt) \
    O(StoreAddr) \
    \
    O(RelToAbs) \
    O(IncMulAP) \
    \
    O(JmpUc) \
    /* don't add here */ \
    O(JmpIntNz) \
    O(JmpIntZe) \
    O(JmpIntEq) \
    O(JmpIntNe) \
    O(JmpIntLt) \
    O(JmpIntGt) \
    O(JmpIntLe) \
    O(JmpIntGe) \
    \
    O(Call) \
    O(Return) \
/**/

typedef enum
{
#define O(m_name) vm_inst_##m_name,
    VM_INST_TABLE
#undef O

    VM_INST_COUNT
} vm_inst_t;

static inline const char *
vm_inst_name(vm_inst_t inst)
{
    switch (inst) {
#define O(m_name) case vm_inst_##m_name: return #m_name; 
    VM_INST_TABLE
#undef O
        default:
            return "<unknown>";
    }
}

typedef struct
{
    i32 integer [2];
    u32 address [2];
    f32 floating[2];
} vm_registers_t;

typedef struct
{
    dck_stretchy_t (u32, u32) code;
    dck_stretchy_t (u8,  u32) memory;

    u32 bp, ip, ap;
    vm_registers_t regs;

    u32 popped_pos;
} vm_t;

vm_t
vm_create(void);

void
vm_init(vm_t *vm, u32 code_offset);

void
vm_clear(vm_t *vm);

void
vm_step(vm_t *vm, cz_t *cz);

b32
vm_is_running(vm_t *vm);

void
vm_execute(vm_t *vm, cz_t *cz, u32 code_offset);

void
vm_push_data(vm_t *vm, u32 alignment, u32 size, void *ptr);
#define VM_PUSH(vm_m, type_m, ...) \
    vm_push_data((vm_m), _Alignof(type_m), sizeof(type_m), (__VA_ARGS__))

void *
vm_get_data(vm_t *vm, u32 alignment, u32 size);
#define VM_GET(vm_m, type_m) \
    ((type_m *)vm_get_data(vm_m, _Alignof(type_m), sizeof(type_m)))

void *
vm_get_arr_data(vm_t *vm, u32 alignment, u32 size, u32 count);
#define VM_GET_ARR(vm_m, type_m, count_m) \
    ((type_m *)vm_get_arr_data(vm_m, _Alignof(type_m), sizeof(type_m), count_m))

b32
vm_print_instruction(vm_t *vm, cz_t *cz);

void
vm_disassemble(vm_t *vm, cz_t *cz, u32 code_offset);

/*
 * Compiler
 */
#define VM_MAX_ALIGNMENT (_Alignof(void *))
#define VM_ADDRESS_ALIGNMENT (_Alignof(u32))

typedef struct
{
    u32 alignment;
    u32 size;
} vm_allocation_t;

typedef struct
{
    u32 prev_mem_off;
    u32 base_offset;
    u32 alignment;
    u32 size;

    variable_t var;
} vm_object_t;

typedef struct
{
    u32 label_index;
    u32 absolute_offset;
} vm_patch_t;

typedef struct
{
    u32 abs_func_index;

    u32 code_offset;

    u32 object_offset;
    u32 in_count;
    u32 meta_count;
    u32 var_count;
    u32 out_count;
} vm_func_t;

typedef struct
{
    u32 func_index;
    u32 code_offset;
} vm_func_ref_t;

typedef struct
{
    u32 object_offset;
    u32 memory_offset;
} vm_scope_offset_t;

typedef struct
{
    u32 allocated_memory;
    dck_stretchy_t (u32, u32) code;

    dck_stretchy_t (vm_object_t, u32) objects;
    dck_stretchy_t (vm_patch_t,  u32) jump_patches;
    dck_stretchy_t (vm_patch_t,  u32) labels;

    dck_stretchy_t (vm_scope_offset_t, u32) scope_offsets;

    dck_stretchy_t (vm_object_t, u32) func_objects;
    dck_stretchy_t (vm_func_t,   u32) funcs;
} vm_compiler_t;

vm_func_ref_t
vm_compile(vm_t *vm, vm_compiler_t *compiler, cz_t *cz, func_ref_t func_ref);

void
vm_call(vm_t *vm, vm_compiler_t *compiler, cz_t *cz, vm_func_ref_t func);

#endif // INTERPRETER_H_
