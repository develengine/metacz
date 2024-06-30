#ifndef INTERPRETER_H_
#define INTERPRETER_H_

#include "metacz.h"

/*
 * Interpreter
 */
typedef enum
{
    vm_inst_Halt = 0,

    vm_inst_IncSP,
    vm_inst_MemMove,

    vm_inst_AddInt,
    vm_inst_SubInt,

    vm_inst_Load,
    // vm_inst_LoadBP,
    vm_inst_LoadImm,
    // vm_inst_LoadAbs,

    vm_inst_Store,
    // vm_inst_StoreBP,
    // vm_inst_StoreImm,
    // vm_inst_StoreAbs,

    // vm_inst_Call,
    // vm_inst_Ret,

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

typedef union
{
    vm_inst_t inst;
    u32 u;
    i32 i;
} vm_code_t;

typedef struct
{
    i32 int_a, int_b;
} vm_registers_t;

typedef struct
{
    dck_stretchy_t (u32, u32) code;
    dck_stretchy_t (u8,  u32) memory;

    vm_registers_t registers;

    u32 bp, ip;

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

b32
vm_print_instruction(vm_t *vm, cz_t *cz);

void
vm_disassemble(vm_t *vm, cz_t *cz, u32 code_offset);

/*
 * Compiler
 */
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

    type_ref_t type_ref;
    b32 is_reference;
} vm_object_t;

typedef struct
{
    u32 label_index;
    u32 absolute_offset;
} vm_patch_t;

typedef struct
{
    u32 allocated_memory;
    dck_stretchy_t (vm_object_t, u32) objects;
    dck_stretchy_t (vm_patch_t,  u32) jump_patches;
    dck_stretchy_t (vm_patch_t,  u32) labels;
} vm_compiler_t;

u32
vm_compile(vm_t *vm, vm_compiler_t *compiler, cz_t *cz, func_ref_t func);

#endif // INTERPRETER_H_
