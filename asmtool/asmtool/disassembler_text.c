#include "gpu.h"
#include "instructions.h"

static void disassemble_data_store(instruction* instruction, char* buffer)
{
    sprintf(buffer, "device_store %d", instruction->data.load_store.reg);
}

static void disassemble_ret(instruction* instruction, char* buffer)
{
    sprintf(buffer, "ret r%d", instruction->data.ret.reg);
}

static void disassemble_mov(instruction* instruction, char* buffer)
{
    sprintf(buffer, "mov r%d, %d", instruction->data.mov.reg, instruction->data.mov.value);
}

bool disassemble_structs_to_text(instruction* instructions, binary_data* text)
{
    validate(text->data == 0, "");
    text->len = 1000;
    text->data = calloc(1000, 1);
    
    int len = 0;
    char buffer[200];
    
    while (instructions)
    {
        switch (instructions->type)
        {
            case INSTRUCTION_STORE:
                disassemble_data_store(instructions, buffer);
                break;
            
            case INSTRUCTION_RET:
                disassemble_ret(instructions, buffer);
                break;
                
            case INSTRUCTION_MOV:
                disassemble_mov(instructions, buffer);
                break;
                
            default:
                error("Unhandled instruction %d\n", instructions->type);
        }
        
        strcat((char*)text->data, buffer);
        strcat((char*)text->data, ";\n");
        len += strlen(buffer) + 2;
        
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
