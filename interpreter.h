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

    vm_inst_AddInt,
    vm_inst_SubInt,

    vm_inst_Load,
    vm_inst_LoadBP,
    vm_inst_LoadSP,
    vm_inst_LoadIP,
    vm_inst_LoadImm,
    vm_inst_LoadAbs,
    vm_inst_LoadGlobal,

    vm_inst_Store,
    vm_inst_StoreBP,
    vm_inst_StoreSP,
    vm_inst_StoreIP,
    vm_inst_StoreImm,
    vm_inst_StoreAbs,
    vm_inst_StoreGlobal,

    vm_inst_Call,
    vm_inst_Ret,

    vm_inst_JmpUc,
    vm_inst_JmpNz,
    vm_inst_JmpZe,
    vm_inst_JmpEq,
    vm_inst_JmpNe,
    vm_inst_JmpLt,
    vm_inst_JmpGt,
    vm_inst_JmpLe,
    vm_inst_JmpGe,

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
    dck_stretchy_t (u32, u32) code;
    dck_stretchy_t (u8,  u32) memory;
    u32 sp, bp, ip;
} vm_t;

void
vm_init(vm_t *vm, u32 code_offset);

void
vm_step(vm_t *vm, cz_t *cz);

b32
vm_is_running(vm_t *vm);

void
vm_execute(vm_t *vm, cz_t *cz, u32 code_offset);

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
    dck_stretchy_t (vm_patch_t,  u32) label_patches;
} vm_compiler_t;

u32
vm_compile(vm_t *vm, vm_compiler_t *compiler, cz_t *cz, func_ref_t func);

#endif // INTERPRETER_H_
