#include "gpu.h"
#include "instructions.h"

typedef struct _function {
    instruction_type type;
    bool (*func)(instruction* instruction, binary_data data, int* size);
} function;

static void SET_BITS(binary_data data, int start, int end, uint64_t value_new)
{
    if (start / 8 >= data.len || end / 8 >= data.len)
    {
        return;
    }
    int64_t mask = ~(((uint64_t)-1) << (end-start + 1));
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

static bool make_memory_offset(operation_src src, int* value, int* flag_sign1, int* flag_sign2)
{
    *flag_sign1 = src.type == OPERATION_SOURCE_IMMEDIATE;
    *flag_sign2 = 0;
    if (*flag_sign1)
    {
        validate(src.flags & OPERATION_FLAG_SIGN_EXTEND, "Must be sign extended");
        *value = src.value_int;
        return true;
    }
    
    validate(src.value_int < 0x100, "");
    validate(src.type == OPERATION_SOURCE_REG32, "Invalid type");
    
    *flag_sign2 = !(src.flags & OPERATION_FLAG_SIGN_EXTEND);

    *value = src.value_int << 1;
    
    return true;
}

static bool make_memory_base(operation_src src, int* value, int* flag)
{
    if (src.type != OPERATION_SOURCE_UNIFORM64 && src.type != OPERATION_SOURCE_REG64)
    {
        error("Invalid type");
    }
    
    *flag = src.type == OPERATION_SOURCE_UNIFORM64;
    *value = src.value_int << 1;
    
    return true;
}

static bool make_memory_reg(operation_src src, int* value, int* flag)
{
    if (src.type != OPERATION_SOURCE_REG32 && src.type != OPERATION_SOURCE_REG16L && src.type != OPERATION_SOURCE_REG16H)
    {
        error("Invalid type");
    }
    
    *flag = src.type == OPERATION_SOURCE_REG32;
    if (*flag)
    {
        *value = src.value_int << 1;
    }
    else
    {
        *value = src.value_int;
    }
    
    return true;
}

static bool make_aludst(operation_src src, uint32_t* value, int* flags)
{
    *flags = 0;
    switch (src.type) {
        case OPERATION_SOURCE_REG32:
            *flags |= 2;
            *value = src.value_int << 1;
            break;
        case OPERATION_SOURCE_REG16L:
            *value = src.value_int << 1;
            break;
        case OPERATION_SOURCE_REG16H:
            *value = (src.value_int << 1) | 1;
            break;
            
        default:
            error("Unhandled operation src %d", src.type);
    }
    return true;
}

static binary_data make_instruction_data(binary_data data, int bytes)
{
    binary_data ret;
    binary_data_sub(data, &ret, 0, bytes);
    return ret;
}

/*
 --------------------------------------------
 Actual assembling start
 --------------------------------------------
 */

static bool assemble_data_store(instruction* instruction, binary_data data, int* size)
{
    instruction_data_load_store *instr = &instruction->data.load_store;
    
    *size = 8;
    data = make_instruction_data(data, *size);
    
    int offset;
    int reg;
    int base;
    int flag_offset_immediate;
    int flag_offset_signextend;
    int flag_base;
    int flag_reg;
    
    make_memory_offset(instr->memory_offset, &offset, &flag_offset_immediate, &flag_offset_signextend);
    
    int shift = 0;
    while (offset % 2 == 0)
    {
        shift++;
        offset >>= 1;
    }
    
    make_memory_base(instr->memory_base , &base, &flag_base);
    make_memory_reg(instr->memory_reg, &reg, &flag_reg);
    
    SET_BITS(data, 0, 6, OPCODE_STORE);
    
    int format = instr->format;
    SET_BITS(data, 7, 9, format);
    format >>= 3;
    SET_BITS(data, 10, 15, reg);
    reg >>= 6;
    SET_BITS(data, 16, 19, base);
    base >>=4;
    SET_BITS(data, 20, 23, offset);
    offset >>=4;
    SET_BITS(data, 24, 24, flag_offset_immediate);
    SET_BITS(data, 25, 25, flag_offset_signextend);
    SET_BITS(data, 27, 27, flag_base);
    SET_BITS(data, 32, 35, offset);
    offset >>=4;
    SET_BITS(data, 36, 39, base);
    SET_BITS(data, 40, 41, reg);
    SET_BITS(data, 42, 43, shift);
    SET_BITS(data, 48, 48, format);
    SET_BITS(data, 49, 49, flag_reg);
    SET_BITS(data, 52, 55, instr->mask);
    SET_BITS(data, 56, 63, offset);
    
    // Unknown
    SET_BITS(data, 26, 26, 1);
    SET_BITS(data, 44, 46, 4);
    
    // TODO Long flag
    SET_BITS(data, 47, 47, 1);
 
    return true;
}

static bool assemble_ret(instruction* instruction, binary_data data, int* size)
{
    *size = 2;
    data = make_instruction_data(data, *size);
    
    SET_BITS(data, 0, 6, OPCODE_RET);
    
    int reg = instruction->data.ret.reg;
    SET_BITS(data, 9, 15, reg);
    
    return true;
}

static bool assemble_mov(instruction* instruction, binary_data data, int* size)
{
    instruction_mov *instr = &instruction->data.mov;
    
    bool flag32 = instr->dest.type == OPERATION_SOURCE_REG32;
    *size = flag32 ? 6 : 4;
    data = make_instruction_data(data, *size);
    
    SET_BITS(data, 0, 6, OPCODE_MOV);
    
    uint32_t reg;
    int flag;
    check(make_aludst(instr->dest, &reg, &flag));
    
    SET_BITS(data, 7, 8, flag);
    
    SET_BITS(data, 9, 14, reg);
    reg >>= 6;
    
    validate(instr->source.type == OPERATION_SOURCE_IMMEDIATE, "");
    int value = instr->source.value_int;
    
    if (flag32)
    {
        SET_BITS(data, 16, 47, value);
        SET_BITS(data, 60, 61, reg);
    }
    else
    {
        SET_BITS(data, 16, 31, value);
        SET_BITS(data, 44, 45, reg);
    }
    
    return true;
}

bool assemble_stop(instruction* instruction, binary_data data, int* size)
{
    *size = 2;
    data = make_instruction_data(data, *size);
    
    SET_BITS(data, 0, 15, 0x88);
    
    return true;
}

static function functions[] =
{
    {INSTRUCTION_STORE, assemble_data_store},
    {INSTRUCTION_RET, assemble_ret},
    {INSTRUCTION_MOV, assemble_mov},
    {INSTRUCTION_STOP, assemble_stop},
};

static bool call_func(instruction* instruction, binary_data data, int* size)
{
    for (int i = 0; i < ARRAY_SIZE(functions); i++)
    {
        if (functions[i].type == instruction->type)
        {
            check(functions[i].func(instruction, data, size));
            return true;
        }
    }
    error("Unhandled instruction %d", instruction->type);
    return false;
}

bool assemble_structs_to_bytecode(instruction* instructions, binary_data* bytecode)
{
    validate(bytecode->data == 0, "Invalid input");
    bytecode->len = 1000;
    bytecode->data = calloc(1000, 1);
    
    int len = 0;
    while (instructions)
    {
        int size;
        binary_data data;
        char buffer[20] = {0};
        data.data = (void*)buffer;
        data.len = 20;
        
        check (call_func(instructions, data, &size));

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
