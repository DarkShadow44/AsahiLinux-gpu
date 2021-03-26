#include "gpu.h"
#include "instructions.h"

static bool get_destination(char* text, int* dest)
{
    validate(text[0] == 'r', "TODO\n");
    *dest = atoi(&text[1]);
    return true;
}

static bool assemble_mov(char buffer[10][20], instruction* instruction)
{
    instruction->type = INSTRUCTION_MOV;
    
    check(get_destination(buffer[1], &instruction->data.mov.reg));
    instruction->data.mov.value = atoi(buffer[2]);
    
    return true;
}

static bool assemble_data_store(char buffer[10][20], instruction* instruction)
{
    instruction->type = INSTRUCTION_STORE;
    
   //TODO check(get_destination(buffer[1], &instruction->data.load_store.reg));
    
    return true;
}

static int split_instruction(char* line, char buffer[10][20])
{
    int pos = 0;
    char *state;
    char* part = strtok_r(line, ", \t\r\n", &state);
    
    while (part)
    {
        strcpy(buffer[pos++], part);
        part = strtok_r(NULL, ", \t\r\n", &state);
    }
    
    return pos;
}

static bool assemble_instruction(char* text, instruction* instruction)
{
    char buffer[10][20] = {0};
    split_instruction(text, buffer);
    
    if (!strcmp(buffer[0], "mov"))
    {
        check(assemble_mov(buffer, instruction));
    }
    else if (!strcmp(buffer[0], "device_store"))
    {
        check(assemble_data_store(buffer, instruction));
    }
    else
    {
        error("Unknown mnemonic '%s'\n", buffer[0]);
    }
    
    return true;
}

bool assemble_text_to_structs(binary_data text, instruction** instructions)
{
    instruction* instruction_last = 0;
    char* state_lines;
    
    char* line = strtok_r((char*)text.data, ";", &state_lines);
    
    while (line)
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
        
        check(assemble_instruction(line, instruction));
        
        line = strtok_r(0, ";", &state_lines);
    }
    return true;
}
