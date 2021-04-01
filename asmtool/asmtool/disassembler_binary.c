#include "gpu.h"
#include "instructions.h"

typedef struct _function {
    int opcode;
    int mask;
    bool (*func)(binary_data data, instruction* instruction, int* size);
} function;

static binary_data get_instruction_data(binary_data data, int bytes)
{
    binary_data ret;
    binary_data_sub(data, &ret, 0, bytes);
    return ret;
}

static operation_src make_memory_offset(int value, int flag_sign1, int flag_sign2)
{
    operation_src ret = {0};
    if (flag_sign1)
    {
        ret.flags = OPERATION_FLAG_SIGN_EXTEND;
        ret.type = OPERATION_SOURCE_IMMEDIATE;
        ret.value_int = value;
        return ret;
    }
    
    assert((value & 1) == 0);
    assert(value < 0x100);
    
    if (!flag_sign2)
    {
        ret.flags = OPERATION_FLAG_SIGN_EXTEND;
    }
    ret.type = OPERATION_SOURCE_REG32;
    ret.value_int = value >> 1;
    
    return ret;
}

static operation_src make_memory_base(int value, int flag)
{
    assert((value & 1) == 0);

    operation_src ret = {0};
    ret.type = flag ? OPERATION_SOURCE_UNIFORM64 : OPERATION_SOURCE_REG64;
    ret.value_int = value >> 1;
    
    return ret;
}

static operation_src make_memory_reg(int value, int flag)
{
    assert((value & 1) == 0);

    operation_src ret = {0};
    if (flag)
    {
        ret.type = OPERATION_SOURCE_REG32;
        ret.value_int = value >> 1;
    }
    else
    {
        ret.type = (value & 1) ? OPERATION_SOURCE_REG16H : OPERATION_SOURCE_REG16L;
        ret.value_int = value >> 1;
    }
    
    return ret;
}

static operation_src make_aludst(int value, int flag)
{
    operation_src ret = {0};
   if (flag & 2)
   {
       ret.type = OPERATION_SOURCE_REG32;
       ret.value_int = value >> 1;
   }
   else
   {
       ret.type = (value & 1) ? OPERATION_SOURCE_REG16H : OPERATION_SOURCE_REG16L;
       ret.value_int = value >> 1;
   }
    return ret;
}

/*
 --------------------------------------------
 Actual disassembling start
 --------------------------------------------
 */

static bool disassemble_data_loadstore(binary_data data, instruction* instruction, int* size)
{
    *size = 8;
    data = get_instruction_data(data, *size); // TODO: L flag
    int format1 = GET_BITS(data, 7, 9);
    int reg1 = GET_BITS(data, 10, 15);
    int base1 = GET_BITS(data, 16, 19);
    int offset1 = GET_BITS(data, 20, 23);
    int flag_offset_immediate = GET_BITS(data, 24, 24);
    int flag_offset_signextend = GET_BITS(data, 25, 25);
    int u1 = GET_BITS(data, 26, 26);
    int flag_base = GET_BITS(data, 27, 27);
    int u3 = GET_BITS(data, 28, 29);
    int u2 = GET_BITS(data, 30, 30);
    // int unk1 = GET_BITS(data, 31, 31);
    int offset2 = GET_BITS(data, 32, 35);
    int base2 = GET_BITS(data, 36, 39);
    int reg2 = GET_BITS(data, 40, 41);
    int shift = GET_BITS(data, 42, 43);
    int u4 = GET_BITS(data, 44, 46);
    // int flag_long = GET_BITS(data, 47, 47);
    int format2 = GET_BITS(data, 48, 48);
    int flag_reg = GET_BITS(data, 49, 49);
    int u5 = GET_BITS(data, 50, 51);
    int mask = GET_BITS(data, 52, 55);
    int offset3 = GET_BITS(data, 56, 63);
    
    assert(u1 == 1);
    assert(u4 == 4);
 
    int offset = offset1 + (offset2 << 4) + (offset3 << 8);
    int reg = reg1 + (reg2 << 6);
    int base = base1 + (base2 << 4);
    int format = format1 + (format2 << 3);
    int u = u1 + (u2 << 1) + (u3 << 2) + (u4 << 4) + (u5 << 7);
    (void)u; // TODO
    
    offset = offset << shift;
    
    instruction_data_load_store* instr = &instruction->data.load_store;
    instr->memory_offset = make_memory_offset(offset, flag_offset_immediate, flag_offset_signextend);
    instr->memory_base = make_memory_base(base, flag_base);
    instr->memory_reg = make_memory_reg(reg, flag_reg);
    instr->format = format;
    instr->mask = mask;
    return true;
}

static bool disassemble_data_load(binary_data data, instruction* instruction, int* size)
{
    instruction->type = INSTRUCTION_LOAD;
    return disassemble_data_loadstore(data, instruction, size);
}

static bool disassemble_data_store(binary_data data, instruction* instruction, int* size)
{
    instruction->type = INSTRUCTION_STORE;
    return disassemble_data_loadstore(data, instruction, size);
}

static bool disassemble_ret(binary_data data, instruction* instruction, int* size)
{
    *size = 2;
    data = get_instruction_data(data, *size); // TODO: L flag
    int reg = GET_BITS(data, 9, 15);
    
    instruction->type = INSTRUCTION_RET;
    instruction_ret* instr = &instruction->data.ret;
    instr->reg = reg;
    return true;
}

static bool disassemble_mov(binary_data data, instruction* instruction, int* size)
{
    int flag = GET_BITS(data, 7, 8);
    int flag_long = GET_BITS(data, 15, 15);
    
    int flag32 = flag & 2;
    *size = flag32 ? 6 : 4;
    if (flag_long)
    {
        *size += 2;
    }
    data = get_instruction_data(data, *size);
    int reg1 = GET_BITS(data, 9, 14);
    int value;
    int reg2;
    if (flag32)
    {
        value = GET_BITS(data, 16, 47);
        reg2 = GET_BITS(data, 60, 61);
    }
    else
    {
        value = GET_BITS(data, 16, 31);
        reg2 = GET_BITS(data, 44, 45);
    }
    int reg = reg1 + (reg2 << 6);
    
    instruction->type = INSTRUCTION_MOV;
    instruction_mov* instr = &instruction->data.mov;
    instr->dest = make_aludst(reg, flag);
    instr->source.type = OPERATION_SOURCE_IMMEDIATE;
    instr->source.value_int = value;
    return true;
}

static bool disassemble_stop(binary_data data, instruction* instruction, int* size)
{
    *size = 2;
    data = get_instruction_data(data, *size);
    int value = GET_BITS(data, 0, 15);
    validate(value == 0x88, "");
    
    instruction->type = INSTRUCTION_STOP;
    return true;
}

static bool disassemble_wait(binary_data data, instruction* instruction, int* size)
{
    *size = 2;
    data = get_instruction_data(data, *size);
    int value = GET_BITS(data, 0, 15);
    validate(value == OPCODE_WAIT, "");
    
    instruction->type = INSTRUCTION_WAIT;
    return true;
}

static function functions[] =
{
    {OPCODE_STORE, 0x7F, disassemble_data_store},
    {OPCODE_LOAD, 0x7F, disassemble_data_load},
    {OPCODE_RET, 0x7F, disassemble_ret},
    {OPCODE_MOV, 0x7F, disassemble_mov},
    {OPCODE_STOP, 0xFFFF, disassemble_stop},
    {OPCODE_WAIT, 0x7F, disassemble_wait},
};

static bool call_func(binary_data data, instruction* instruction, int* size)
{
    int len = 32;
    if (data.len < 4)
    {
        len = data.len * 8;
    }
    int opcode = GET_BITS(data, 0, len - 1);
    
    for (int i = 0; i < ARRAY_SIZE(functions); i++)
    {
        int masked = opcode & functions[i].mask;
        if (functions[i].opcode == masked)
        {
            check(functions[i].func(data, instruction, size));
            return true;
        }
    }
    printf("Last instruction: \n");
    dump_to_hex(data.data, data.len < 10 ? data.len : 10);
    error("Unknown opcode 0x%0X\n", opcode);
    return false;
}


bool disassemble_bytecode_to_structs(binary_data bytecode, instruction** instructions)
{
    int size;
    int pos = 0;
    
    *instructions = 0;
    
    instruction* instruction_last = 0;
    
    while (pos < bytecode.len)
    {
        instruction* instruction = init_instruction();
        if (instruction_last == 0)
        {
            *instructions = instruction;
            instruction_last = instruction;
        }
        else
        {
            instruction_last->next = instruction;
            instruction_last = instruction;
        }
        
        binary_data data_instruction;
        binary_data_sub(bytecode, &data_instruction, pos, bytecode.len - pos);
        
        check(call_func(data_instruction, instruction, &size));
        
        instruction->original_len = size;
        memcpy(instruction->original_bytes, data_instruction.data, size);
        pos += size;
    }
    
    return true;
}
