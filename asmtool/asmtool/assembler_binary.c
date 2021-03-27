#include "gpu.h"
#include "instructions.h"

void SET_BITS(binary_data data, int start, int end, uint64_t value_new)
{
    assert(start / 8 < data.len && end / 8 < data.len);
    int64_t mask = ~((-1) << (end-start + 1));
    value_new &= mask;

    int start_full = start / 8;
    int start_part = start % 8;
    
    unsigned char* bytes = data.data + start_full;
    
    // Extract up to 7 bytes of data
    int64_t value = 0;
    for (int i = 0; i < 7 && i < data.len; i++)
    {
        value += (int64_t)bytes[i] << (i*8);
    }
    
    value |= value_new << start_part;
    
    for (int i = 0; i < 7 && i < data.len; i++)
    {
        bytes[i] = (value >> (i*8)) & 0xFF;
    }
}

static binary_data make_instruction_data(binary_data data, int bytes)
{
    binary_data ret;
    binary_data_sub(data, &ret, 0, bytes);
    return ret;
}

bool assemble_data_store(instruction* instruction, binary_data data, int* size)
{
    return true;
}

bool assemble_ret(instruction* instruction, binary_data data, int* size)
{
    *size = 2;
    data = make_instruction_data(data, *size);
    
    SET_BITS(data, 0, 6, OPCODE_RET);
    
    int reg = instruction->data.ret.reg;
    SET_BITS(data, 9, 15, reg);
    
    return true;
}

bool assemble_mov(instruction* instruction, binary_data data, int* size)
{
    *size = 6;
    data = make_instruction_data(data, *size);
    
    SET_BITS(data, 0, 6, OPCODE_MOV);
    
    int reg = instruction->data.mov.reg << 1;
    SET_BITS(data, 9, 14, reg);
    reg >>= 6;
    SET_BITS(data, 44, 45, reg);
    
    int value = instruction->data.mov.reg;
    SET_BITS(data, 16, 31, value);
    
    return true;
}

bool assemble_structs_to_bytecode(instruction* instructions, binary_data* bytecode)
{
    validate(bytecode->data == 0, "");
    bytecode->len = 1000;
    bytecode->data = calloc(1000, 1);
    
    char buffer[20];
    
    int len = 0;
    while (instructions)
    {
        int size;
        binary_data data;
        data.data = (void*)buffer;
        data.len = 20;
        switch (instructions->type)
        {
            case INSTRUCTION_STORE:
                check(assemble_data_store(instructions, data, &size));
                break;

            case INSTRUCTION_RET:
                check(assemble_ret(instructions, data, &size));
                break;

            case INSTRUCTION_MOV:
                check(assemble_mov(instructions, data, &size));
                break;

            default:
                error("Unhandled instruction %d", instructions->type);
        }

        memcpy(bytecode->data + len, buffer, size);
        len += size;

        instructions = instructions->next;
        if (len + 20 > bytecode->len)
        {
            bytecode->len *= 2;
            bytecode->data = realloc(bytecode->data, bytecode->len);
        }
    }
    bytecode->len = len;
    return true;
}
