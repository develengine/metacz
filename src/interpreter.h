#ifndef INTERPRETER_H_
#define INTERPRETER_H_

#include "metacz.h"

/*
 * Interpreter
 */
typedef enum
{
    vm_inst_Halt = 0,

    vm_inst_IncSP,      // () -> ()
    vm_inst_MemMove,    // ([u32], [u32], [u32]) -> (a)

    vm_inst_AddInt,
    vm_inst_SubInt,

    vm_inst_Load,
    vm_inst_LoadImm,
    vm_inst_LoadAbs,
    vm_inst_LoadAP,

    vm_inst_LoadInt,

    vm_inst_Store,
    vm_inst_StoreAbs,
    vm_inst_StoreAP,

    vm_inst_StoreInt,

    vm_inst_RelToAbs,
    vm_inst_IncMulAP,

    vm_inst_JmpUc,
    /* don't add here */
    vm_inst_JmpIntNz,
    vm_inst_JmpIntZe,
    vm_inst_JmpIntEq,
    vm_inst_JmpIntNe,
    vm_inst_JmpIntLt,
    vm_inst_JmpIntGt,
    vm_inst_JmpIntLe,
    vm_inst_JmpIntGe,

    VM_INST_COUNT
} vm_inst_t;

typedef struct
{
    i32 integer [2];
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
    func_ref_t func_ref;

    u32 code_offset;

    u32 object_offset;
    u32 in_count;
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
    u32 allocated_memory;
    dck_stretchy_t (vm_object_t, u32) objects;
    dck_stretchy_t (vm_patch_t,  u32) jump_patches;
    dck_stretchy_t (vm_patch_t,  u32) labels;

    dck_stretchy_t (vm_object_t, u32) func_objects;
    dck_stretchy_t (vm_func_t,   u32) funcs;
} vm_compiler_t;

vm_func_ref_t
vm_compile(vm_t *vm, vm_compiler_t *compiler, cz_t *cz, func_ref_t func_ref);

#endif // INTERPRETER_H_
