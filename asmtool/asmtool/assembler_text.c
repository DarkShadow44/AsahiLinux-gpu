#include "gpu.h"
#include "instructions.h"

typedef struct _function {
    const char* type;
    bool (*func)(char buffer[10][20], instruction* instruction);
} function;

static int split_instruction(char* line, char buffer[10][20])
{
    int pos = 0;
    char *state;
    char* part = strtok_r(line, ", \t", &state);
    
    while (part)
    {
        strcpy(buffer[pos++], part);
        part = strtok_r(NULL, ", \t", &state);
    }
    
    return pos;
}

static bool make_operation_src(char* text, operation_src* src)
{
    char* state;
    char* reg1 = strtok_r(text, "_", &state);
    char* reg2 = strtok_r(NULL, "_", &state);
    src->flags = 0;
    if (reg1[0] == 'r')
    {
        src->type = OPERATION_SOURCE_REG32;
        src->value_int = atoi(&reg1[1]);
        
        if (reg2)
        {
            src->type = OPERATION_SOURCE_REG64;
            validate(reg2[0] == 'r', "Second part must be register as well!");
            int value2 = atoi(&reg2[1]);
            validate(value2 == src->value_int  + 1, "Second part must be subsequent register!");
        }
        else
        {
            char last_char = reg1[strlen(reg1) - 1];
            if (last_char == 'l')
            {
                src->type = OPERATION_SOURCE_REG16L;
            }
            if (last_char == 'h')
            {
                src->type = OPERATION_SOURCE_REG16H;
            }
        }
    }
    else if (reg1[0] == 'u')
    {
        src->type = OPERATION_SOURCE_REG32;
        src->value_int = atoi(&reg1[1]);
        
        if (reg2)
        {
            src->type = OPERATION_SOURCE_UNIFORM64;
            validate(reg2[0] == 'u', "Second part must be uniform as well!");
            int value2 = atoi(&reg2[1]);
            validate(value2 == src->value_int  + 1, "Second part must be subsequent uniform!");
        }
        else
        {
            assert(0);
        }
    }
    else
    {
        src->type = OPERATION_SOURCE_IMMEDIATE;
        src->value_int = atoi(text);
    }
    
    return true;
}

static bool get_memoryformat(char* format, int* value)
{
    for (int i = 0; i < ARRAY_SIZE(format_names); i++)
    {
        if (!strcmp(format, format_names[i]))
        {
            *value = i;
            return true;
        }
    }
    error("Unknown format %s\n", format);
}

// TODO: Sanity check against mask
static bool make_memory_reg(char* buffer, operation_src* memory_reg, int mask)
{
    char* state;
    char* reg = strtok_r(buffer, "_", &state);
    
    check(make_operation_src(reg, memory_reg));
    return true;
}

/*
 --------------------------------------------
 Actual assembling start
 --------------------------------------------
 */

static bool assemble_data_loadstore(char buffer[10][20], instruction* instruction)
{
    instruction_data_load_store* instr = &instruction->data.load_store;
    
    check(get_memoryformat(buffer[1], & instr->format));
    instr->mask = (int)strtol(buffer[2], NULL, 16);
    
    check(make_memory_reg(buffer[3], &instr->memory_reg, instr->mask));
    check(make_operation_src(buffer[4], &instr->memory_base));
    check(make_operation_src(buffer[5], &instr->memory_offset));
          
    bool flag_signed = strcmp("signed", buffer[6]) == 0;
    if (flag_signed)
    {
        instr->memory_offset.flags |= OPERATION_FLAG_SIGN_EXTEND;
    }
    
    return true;
}

static bool assemble_data_store(char buffer[10][20], instruction* instruction)
{
    instruction->type = INSTRUCTION_STORE;
    return assemble_data_loadstore(buffer, instruction);
}

static bool assemble_data_load(char buffer[10][20], instruction* instruction)
{
    instruction->type = INSTRUCTION_LOAD;
    return assemble_data_loadstore(buffer, instruction);
}

static bool assemble_mov(char buffer[10][20], instruction* instruction)
{
    instruction->type = INSTRUCTION_MOV;
    instruction_mov* instr = &instruction->data.mov;
    
    check(make_operation_src(buffer[1], &instr->dest));
    check(make_operation_src(buffer[2], &instr->source));
    
    return true;
}

static bool assemble_stop(char buffer[10][20], instruction* instruction)
{
    instruction->type = INSTRUCTION_STOP;
    
    return true;
}

static bool assemble_wait(char buffer[10][20], instruction* instruction)
{
    instruction->type = INSTRUCTION_WAIT;
    
    return true;
}

static function functions[] =
{
    {"device_store", assemble_data_store},
    {"device_load", assemble_data_load},
    {"mov", assemble_mov},
    {"stop", assemble_stop},
    {"wait", assemble_wait},
};

static bool call_func(char* text, instruction* instruction)
{
    char buffer[10][20] = {0};
    split_instruction(text, buffer);

    for (int i = 0; i < ARRAY_SIZE(functions); i++)
    {
        if (!strcmp(functions[i].type, buffer[0]))
        {
            check(functions[i].func(buffer, instruction));
            return true;
        }
    }
    error("Unknown mnemonic '%s'\n", buffer[0]);
    return false;
}

bool assemble_text_to_structs(binary_data text, instruction** instructions)
{
    instruction* instruction_last = 0;
    char* state_lines;
    
    char* line = strtok_r((char*)text.data, ";\r\n", &state_lines);
    
    while (line)
    {
        if (strlen(line) == 0)
        {
            continue;
        }
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
        
        check(call_func(line, instruction));
        
        line = strtok_r(0, ";\r\n", &state_lines);
    }
    return true;
}
