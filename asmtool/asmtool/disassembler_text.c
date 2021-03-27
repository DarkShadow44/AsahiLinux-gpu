#include "gpu.h"
#include "instructions.h"

static bool make_operation_src_(char* buffer, operation_src src, int offset)
{
    switch (src.type)
    {
        case OPERATION_SOURCE_REG32:
            sprintf(buffer, "r%d", src.value + offset);
            break;
        case OPERATION_SOURCE_REG64:
            sprintf(buffer, "r%d_r%d", src.value, src.value + 1);
            break;
        case OPERATION_SOURCE_IMMEDIATE:
            sprintf(buffer, "%d", src.value);
            break;
        default:
            error("Unknown type %d\n", src.type);
    }
    
    return true;
}

static bool make_operation_src(char* buffer, operation_src src)
{
    return make_operation_src_(buffer, src, 0);
}

static bool make_memory_reg(char* buffer, operation_src memory_reg, int mask)
{
    buffer[0] = 0;
    
    int num = count_bits(mask);
    for (int i = 0; i < num; i++)
    {
        if (i != 0)
        {
            strcat(buffer, "_");
        }
        char buffer2[20];
        check(make_operation_src_(buffer2, memory_reg, i));
        strcat(buffer, buffer2);
    }
    return true;
}

static void bytes_to_hex(char* buffer, unsigned char* data, int len)
{
    buffer[0] = 0;
    for (int i = 0; i < len; i++)
    {
        char temp[20];
        sprintf(temp, "%02X", data[i]);
        strcat(buffer, temp);
    }
}


static bool disassemble_data_store(instruction* instruction, char* buffer)
{
    char buffer_reg[40];
    char buffer_base[40];
    char buffer_offset[40];
    
    instruction_data_load_store instr = instruction->data.load_store;
    
    check(make_memory_reg(buffer_reg, instr.memory_reg, instr.mask));
    check(make_operation_src(buffer_base, instr.memory_base));
    check(make_operation_src(buffer_offset, instr.memory_offset));
    
    bool flag_signed = instr.memory_offset.flags & OPERATION_FLAG_SIGN_EXTEND;
    
    sprintf(buffer, "device_store %s, 0x%X, %s, %s, %s, %s", format_names[instr.format], instr.mask, buffer_reg, buffer_base, buffer_offset, flag_signed ? "signed" : "unsigned");
    
    return true;
}

static bool disassemble_ret(instruction* instruction, char* buffer)
{
    sprintf(buffer, "ret r%d", instruction->data.ret.reg);
    return true;
}

static bool disassemble_mov(instruction* instruction, char* buffer)
{
    char buffer_src[40];
    char buffer_dst[40];
    
    instruction_mov instr = instruction->data.mov;
    
    check(make_operation_src(buffer_src, instr.source));
    check(make_operation_src(buffer_dst, instr.dest));
    
    sprintf(buffer, "mov %s, %s", buffer_dst, buffer_src);
    
    return true;
}

static bool disassemble_stop(instruction* instruction, char* buffer)
{
    sprintf(buffer, "stop");
    
    return true;
}

bool disassemble_structs_to_text(instruction* instructions, binary_data* text, bool print_offsets)
{
    validate(text->data == 0, "");
    text->len = 1000;
    text->data = calloc(1000, 1);
    
    int bytecode_pos = 0;
    
    int len = 0;
    char buffer[200];
    
    while (instructions)
    {
        switch (instructions->type)
        {
            case INSTRUCTION_STORE:
                check(disassemble_data_store(instructions, buffer));
                break;
            
            case INSTRUCTION_RET:
                check(disassemble_ret(instructions, buffer));
                break;
                
            case INSTRUCTION_MOV:
                check(disassemble_mov(instructions, buffer));
                break;
                
            case INSTRUCTION_STOP:
                check(disassemble_stop(instructions, buffer));
                break;
                
            default:
                error("Unhandled instruction %d", instructions->type);
        }
        
        char line[200];
        if (print_offsets)
        {
            char buffer_bytes[50];
            bytes_to_hex(buffer_bytes, instructions->original_bytes, instructions->original_len);
            sprintf(line, "%4X: %-24s %s;\n", bytecode_pos, buffer_bytes, buffer);
            bytecode_pos += instructions->original_len;
        }
        else
        {
            sprintf(line, "%s;\n", buffer);
        }
        
        strcat((char*)text->data, line);
        len += strlen(line);
        
        instructions = instructions->next;
        if (len + 200 > text->len)
        {
            text->len *= 2;
            text->data = realloc(text->data, text->len);
        }
    }
    text->len = len + 1;
    return true;
}
