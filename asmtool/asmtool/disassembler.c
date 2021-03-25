#include "gpu.h"
#include "instructions.h"

int64_t GET_BITS64(binary_data data, int start, int end)
{
    int start_full = start/8;
    int start_part = start%8;
    
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

static void disassemble_instr_uniform_store(binary_data data)
{
    int to1 = GET_BITS(data, 20, 23);
    int to2 = GET_BITS(data, 32, 35);
    int shift = GET_BITS(data, 42, 43);
    int to3 = GET_BITS(data, 56, 63);
 
    int to = to1 + (to2 << 4) + (to3 << 8);
    to = to << shift;
    
    
    printf("uniform_store to %d\n", to);
}

void disassemble_bytecode_to_structs(binary_data data)
{
    binary_data instruction;
    binary_data_sub(data, &instruction, 6, 8);
    int opcode = GET_BITS(instruction, 0, 6);
    switch (opcode)
    {
        case OPCODE_UNIFORM_STORE:
            disassemble_instr_uniform_store(instruction);
            break;
        default:
            printf("Unknown opcode %d\n", opcode);
    }
}
