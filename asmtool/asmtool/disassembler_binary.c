#include "gpu.h"
#include "instructions.h"

int64_t GET_BITS64(binary_data data, int start, int end)
{
    if (end / 8 >= data.len)
    {
        assert(start / 8 >= data.len);
        return 0;
    }
    int start_full = start / 8;
    int start_part = start % 8;
    
    unsigned char* bytes = data.data + start_full;
    
    // Extract up to 7 bytes of data
    int64_t value = 0;
    for (int i = 0; i < 7 && i < data.len; i++)
    {
        value += (int64_t)bytes[i] << (i*8);
    }
    
    int64_t mask = ~((-1) << (end-start + 1));
    
    return (value >> start_part) & mask;
}

int GET_BITS(binary_data data, int start, int end)
{
    return (int)GET_BITS64(data, start, end);
}

static binary_data get_instruction_data(binary_data data, int bytes)
{
    binary_data ret;
    binary_data_sub(data, &ret, 0, bytes);
    return ret;
}

operation_src make_memory_offset(int value, int flag_sign1, int flag_sign2)
{
    operation_src ret = {0};
    if (flag_sign1)
    {
        ret.flags = OPERATION_FLAG_SIGN_EXTEND;
        ret.type = OPERATION_SOURCE_IMMEDIATE;
        ret.value = value;
        return ret;
    }
    
    assert((value & 1) == 0);
    assert(value < 0x100);
    
    if (!flag_sign2)
    {
        ret.flags = OPERATION_FLAG_SIGN_EXTEND;
    }
    ret.type = OPERATION_SOURCE_REG32;
    ret.value = value >> 1;
    
    return ret;
}

operation_src make_memory_base(int value, int flag)
{
    assert((value & 1) == 0);

    operation_src ret = {0};
    ret.type = flag ? OPERATION_SOURCE_UNIFORM64 : OPERATION_SOURCE_REG64;
    ret.value = value >> 1;
    
    return ret;
}

operation_src make_memory_reg(int value, int flag)
{
    assert((value & 1) == 0);

    operation_src ret = {0};
    if (flag)
    {
        ret.type = OPERATION_SOURCE_REG32;
        ret.value = value >> 1;
    }
    else
    {
        ret.type = OPERATION_SOURCE_REG16;
        ret.value = value;
    }
    
    return ret;
}

operation_src make_aludst(int value, int flag)
{
    operation_src ret = {0};
   if (flag & 2)
   {
       ret.type = OPERATION_SOURCE_REG32;
       ret.value = value >> 1;
   }
   else
   {
       ret.type = OPERATION_SOURCE_REG16;
       ret.value = value;
   }
    return ret;
}

/*
 --------------------------------------------
 Actual disassembling start
 --------------------------------------------
 */

static bool disassemble_data_store(binary_data data, instruction* instruction, int* size)
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
    
    instruction->type = INSTRUCTION_STORE;
    instruction_data_load_store* instr = &instruction->data.load_store;
    instr->memory_offset = make_memory_offset(offset, flag_offset_immediate, flag_offset_signextend);
    instr->memory_base = make_memory_base(base, flag_base);
    instr->memory_reg = make_memory_reg(reg, flag_reg);
    instr->format = format;
    instr->mask = mask;
    return true;
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
    *size = 6;
    data = get_instruction_data(data, *size); // TODO: L flag
    int flag = GET_BITS(data, 7, 8);
    int reg1 = GET_BITS(data, 9, 14);
    int reg2 = GET_BITS(data, 44, 45);
    int reg = reg1 + (reg2 << 6);
    int value = GET_BITS(data, 16, 31);
    
    instruction->type = INSTRUCTION_MOV;
    instruction_mov* instr = &instruction->data.mov;
    instr->dest = make_aludst(reg, flag);
    instr->source.type = OPERATION_SOURCE_IMMEDIATE;
    instr->source.value = value;
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

bool disassemble_bytecode_to_structs(binary_data bytecode, instruction** instructions)
{
    int size;
    int pos = 0;
    
    *instructions = 0;
    
    instruction* instruction_last = 0;
    
    while (pos < bytecode.len)
    {
        binary_data data_opcode;
        binary_data_sub(bytecode, &data_opcode, pos, 1);
        int opcode = GET_BITS(data_opcode, 0, 6);
        
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
        
        switch (opcode)
        {
            case OPCODE_STORE:
                check(disassemble_data_store(data_instruction, instruction, &size));
                break;
            case OPCODE_RET:
                check(disassemble_ret(data_instruction, instruction, &size));
                break;
            case OPCODE_MOV:
                check(disassemble_mov(data_instruction, instruction, &size));
                break;
            case OPCODE_STOP:
                check(disassemble_stop(data_instruction, instruction, &size));
                break;
            default:
                error("Unknown opcode %d", opcode);
        }
        instruction->original_len = size;
        memcpy(instruction->original_bytes, data_instruction.data, size);
        pos += size;
    }
    
    return true;
}
