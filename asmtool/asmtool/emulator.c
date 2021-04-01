#include "gpu.h"
#include "instructions.h"

typedef struct _function {
    instruction_type type;
    bool (*func)(emu_state *state, instruction* instruction);
} function;

static bool get_value_(emu_state *state, operation_src src, uint64_t* value, int offset)
{
    switch (src.type)
    {
        case OPERATION_SOURCE_IMMEDIATE:
            *value = src.value_int;
            break;
        case OPERATION_SOURCE_REG32:
            *value = state->reg[src.value_int + offset];
            break;
        default:
            error("Unhandled src %d", src.type);
    }
    return true;
}

static bool get_value(emu_state *state, operation_src src, uint64_t* value)
{
    return get_value_(state, src, value, 0);
}

static bool put_value_(emu_state *state, operation_src src, uint64_t value, int offset)
{
    uint16_t* ptr16 = (uint16_t*)&state->reg[src.value_int];
    switch (src.type)
    {
        case OPERATION_SOURCE_IMMEDIATE:
            error("Invalid operation");
        case OPERATION_SOURCE_REG32:
            state->reg[src.value_int + offset] = (uint32_t)value;
            break;
        case OPERATION_SOURCE_REG16H:
            ptr16[1] = (uint16_t)value;
            break;
        case OPERATION_SOURCE_REG16L:
            ptr16[0] = (uint16_t)value;
            break;
        default:
            error("Unhandled src %d", src.type);
    }
    return true;
}

static bool put_value(emu_state *state, operation_src src, uint64_t value)
{
    return put_value_(state, src, value, 0);
}

/*
 --------------------------------------------
 Actual emulation start
 --------------------------------------------
 */

static bool emulate_data_loadstore(emu_state *state, instruction* instruction, bool isload)
{
    instruction_data_load_store instr = instruction->data.load_store;
    
    int count = count_bits(instr.mask);
    
    uint64_t offset;
    check(get_value(state, instr.memory_offset, &offset));
    
    switch(instr.format)
    {
        case FORMAT_I32:
            assert(instr.memory_reg.type == OPERATION_SOURCE_REG32);
            for (int i = 0; i < count; i++)
            {
                uint64_t value;
                uint32_t* buffer = (uint32_t*)state->data.buffer0;
                if (isload)
                {
                    value = buffer[offset + i];
                    put_value_(state, instr.memory_reg, value, i);
                }
                else
                {
                    check(get_value_(state, instr.memory_reg, &value, i));
                    buffer[offset + i] = (uint32_t)value;
                }
                
            }
            break;
        default:
            error("Unhandledformat %d", instr.format);
    }
    
    return true;
}

static bool emulate_data_store(emu_state *state, instruction* instruction)
{
    return emulate_data_loadstore(state, instruction, false);
}

static bool emulate_data_load(emu_state *state, instruction* instruction)
{
    return emulate_data_loadstore(state, instruction, true);
}

static bool emulate_mov(emu_state *state, instruction* instruction)
{
    instruction_mov instr = instruction->data.mov;
    uint64_t value;
    check(get_value(state, instr.source, &value));
    check(put_value(state, instr.dest, value));
    return true;
}

static bool emulate_wait(emu_state *state, instruction* instruction)
{
    return true;
}

static function functions[] =
{
    {INSTRUCTION_STORE, emulate_data_store},
    {INSTRUCTION_LOAD, emulate_data_load},
    {INSTRUCTION_MOV, emulate_mov},
    {INSTRUCTION_WAIT, emulate_wait},
};

static bool call_func(emu_state *state, instruction* instruction)
{
    for (int i = 0; i < ARRAY_SIZE(functions); i++)
    {
        if (functions[i].type == instruction->type)
        {
            check(functions[i].func(state, instruction));
            return true;
        }
    }
    error("Unhandled instruction %d", instruction->type);
    return false;
}

bool emulate_instructions(emu_state *state, instruction* instructions)
{
    validate(state->reg[0] == 0, "");
    
    while (instructions)
    {
        if (instructions->type == INSTRUCTION_RET || instructions->type == INSTRUCTION_STOP)
        {
            return true;
        }
        check(call_func(state, instructions));

        instructions = instructions->next;
    }
    return true;
}
