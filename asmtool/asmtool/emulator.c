#include "gpu.h"
#include "instructions.h"

typedef struct _function {
    instruction_type type;
    bool (*func)(emu_state *state, instruction* instruction);
} function;

static bool get_value_(emu_state *state, operation_src src, int64_t* value, int offset)
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

static bool get_value(emu_state *state, operation_src src, int64_t* value)
{
    return get_value_(state, src, value, 0);
}

static bool put_value_(emu_state *state, operation_src src, int64_t value, int offset)
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

static bool put_value(emu_state *state, operation_src src, int64_t value)
{
    return put_value_(state, src, value, 0);
}

static float int_to_float32(int64_t value)
{
    float ret;
    memcpy(&ret, &value, 4);
    return ret;
}

static int64_t float32_to_int(float value)
{
    uint32_t ret;
    memcpy(&ret, &value, 4);
    return ret;
}

static void set_buffer(emu_state *state, int64_t start, int type_size, int64_t value)
{
    memcpy((char*)state->data.buffer0 + start * type_size, &value, type_size);
}

static int64_t get_buffer(emu_state *state, int64_t start, int type_size, bool issigned)
{
    int64_t ret = issigned ? -1 : 0;
    memcpy(&ret, (char*)state->data.buffer0 + start * type_size, type_size);
    return ret;
}

/*
 --------------------------------------------
 Actual emulation start
 --------------------------------------------
 */

typedef int64_t (*loadstore_processor)(int64_t value, bool isload);

static bool emulate_data_loadstore_helper(emu_state *state, instruction_data_load_store instr, bool isload, int type_size, loadstore_processor proc, bool issigned)
{
    int64_t offset;
    check(get_value(state, instr.memory_offset, &offset));
    
    int pos_reg = 0;
    for (int i = 0; i < 4; i++)
    {
        int active_bit = 1 << i;
        if (instr.mask & active_bit)
        {
            if (isload)
            {
                int64_t value = get_buffer(state, offset + i, type_size, issigned);
                if (proc)
                {
                    value = proc(value, true);
                }
                put_value_(state, instr.memory_reg, value, pos_reg);
            }
            else
            {
                int64_t value;
                check(get_value_(state, instr.memory_reg, &value, pos_reg));
                if (proc)
                {
                    value = proc(value, false);
                }
                set_buffer(state, offset + i, type_size, value);
            }
            pos_reg++;
        }
    }
    return true;
}

static int64_t loadstore_processor_u8norm(int64_t value, bool isload)
{
    if (isload)
    {
        return float32_to_int((float)value / 0xFF);
    }
    else
    {
        return int_to_float32(value) * 0xFF;
    }
}

static int64_t loadstore_processor_s8norm(int64_t value, bool isload)
{
    if (isload)
    {
        return float32_to_int((float)value / 0x7F);
    }
    else
    {
        return int_to_float32(value) * 0x7F;
    }
}

static int64_t loadstore_processor_u16norm(int64_t value, bool isload)
{
    if (isload)
    {
        return float32_to_int((float)value / 0xFFFF);
    }
    else
    {
        return int_to_float32(value) * 0xFFFF;
    }
}

static int64_t loadstore_processor_s16norm(int64_t value, bool isload)
{
    if (isload)
    {
        return float32_to_int((float)value / 0x7FFF);
    }
    else
    {
        return int_to_float32(value) * 0x7FFF;
    }
}

static bool emulate_data_loadstore(emu_state *state, instruction* instruction, bool isload)
{
    instruction_data_load_store instr = instruction->data.load_store;
    
    switch(instr.format)
    {
        case FORMAT_I8:
            check(emulate_data_loadstore_helper(state, instr, isload, 1, NULL, false));
            break;
        case FORMAT_I16:
            check(emulate_data_loadstore_helper(state, instr, isload, 2, NULL, false));
            break;
        case FORMAT_I32:
            check(emulate_data_loadstore_helper(state, instr, isload, 4, NULL, false));
            break;
        case FORMAT_U8NORM:
            check(emulate_data_loadstore_helper(state, instr, isload, 1, loadstore_processor_u8norm, false));
            break;
        case FORMAT_S8NORM:
            check(emulate_data_loadstore_helper(state, instr, isload, 1, loadstore_processor_s8norm, true));
            break;
        case FORMAT_U16NORM:
            check(emulate_data_loadstore_helper(state, instr, isload, 2, loadstore_processor_u16norm, false));
            break;
        case FORMAT_S16NORM:
            check(emulate_data_loadstore_helper(state, instr, isload, 2, loadstore_processor_s16norm, true));
            break;
        default:
            error("Unhandled format %d", instr.format);
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
    int64_t value;
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
