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

static bool disassemble_instr_data_store(binary_data data, instruction* instruction, int* size)
{
    *size = 8;
    data = get_instruction_data(data, *size); // TODO: L flag
    int reg1 = GET_BITS(data, 20, 23);
    int reg2 = GET_BITS(data, 32, 35);
    int shift = GET_BITS(data, 42, 43);
    int reg3 = GET_BITS(data, 56, 63);
 
    int reg = reg1 + (reg2 << 4) + (reg3 << 8);
    reg = reg << shift;
    
    instruction->type = INSTRUCTION_STORE;
    instruction->data.load_store.reg = reg;
    return true;
}

static bool disassemble_instr_ret(binary_data data, instruction* instruction, int* size)
{
    *size = 2;
    data = get_instruction_data(data, *size); // TODO: L flag
    int reg = GET_BITS(data, 9, 15);
    
    instruction->type = INSTRUCTION_RET;
    instruction->data.ret.reg = reg;
    return true;
}

static bool disassemble_instr_mov(binary_data data, instruction* instruction, int* size)
{
    *size = 6;
    data = get_instruction_data(data, *size); // TODO: L flag
    int reg1 = GET_BITS(data, 9, 14);
    int reg2 = GET_BITS(data, 44, 45);
    int reg = reg1 + (reg2 << 6);
    int value = GET_BITS(data, 16, 31);
    
    instruction->type = INSTRUCTION_MOV;
    instruction->data.mov.reg = reg >> 1;
    instruction->data.mov.value = value;
    return true;
}

bool disassemble_bytecode_to_structs(binary_data data, instruction** instructions)
{
    int size;
    int pos = 0;
    
    *instructions = 0;
    
    instruction* instruction_last = 0;
    
    while (pos < data.len)
    {
        binary_data data_opcode;
        binary_data_sub(data, &data_opcode, pos, 1);
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
        binary_data_sub(data, &data_instruction, pos, data.len - pos);
        
        switch (opcode)
        {
            case OPCODE_STORE:
                if (!disassemble_instr_data_store(data_instruction, instruction, &size))
                    return false;
                break;
            case OPCODE_RET:
                if (!disassemble_instr_ret(data_instruction, instruction, &size))
                    return false;
                break;
            case OPCODE_MOV:
                if (!disassemble_instr_mov(data_instruction, instruction, &size))
                    return false;
                break;
            default:
                error("Unknown opcode %d\n", opcode);
        }
        pos += size;
    }
    
    return true;
}
