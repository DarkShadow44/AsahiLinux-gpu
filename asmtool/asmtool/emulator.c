#include "gpu.h"
#include "instructions.h"
#include "flexfloat.h"

/* Start source https://cgit.freedesktop.org/mesa/mesa/tree/src/util/format_srgb.h */

/************************************************************************
 *
 * Copyright 2010 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 */

static float util_format_srgb_to_linear_float(float cs)
{
   if (cs <= 0.0f)
      return 0.0f;
   else if (cs <= 0.04045f)
      return cs / 12.92f;
   else if (cs < 1.0f)
      return powf((cs + 0.055) / 1.055f, 2.4f);
   else
      return 1.0f;
}

static float util_format_linear_to_srgb_float(float cl)
{
   if (cl <= 0.0f)
      return 0.0f;
   else if (cl < 0.0031308f)
      return 12.92f * cl;
   else if (cl < 1.0f)
      return 1.055f * powf(cl, 0.41666f) - 0.055f;
   else
      return 1.0f;
}

/* End source */

/* Start source https://cgit.freedesktop.org/mesa/mesa/tree/src/util/format_rgb9e5.h */

#define MIN2( A, B )   ( (A)<(B) ? (A) : (B) )
#define MAX2( A, B )   ( (A)>(B) ? (A) : (B) )

#define MIN3( A, B, C ) ((A) < (B) ? MIN2(A, C) : MIN2(B, C))
#define MAX3( A, B, C ) ((A) > (B) ? MAX2(A, C) : MAX2(B, C))


#define RGB9E5_EXPONENT_BITS          5
#define RGB9E5_MANTISSA_BITS          9
#define RGB9E5_EXP_BIAS               15
#define RGB9E5_MAX_VALID_BIASED_EXP   31

#define MAX_RGB9E5_EXP               (RGB9E5_MAX_VALID_BIASED_EXP - RGB9E5_EXP_BIAS)
#define RGB9E5_MANTISSA_VALUES       (1<<RGB9E5_MANTISSA_BITS)
#define MAX_RGB9E5_MANTISSA          (RGB9E5_MANTISSA_VALUES-1)
#define MAX_RGB9E5                   (((float)MAX_RGB9E5_MANTISSA)/RGB9E5_MANTISSA_VALUES * (1<<MAX_RGB9E5_EXP))

static int rgb9e5_ClampRange(float x)
{
   union { float f; uint32_t u; } f, max;
   f.f = x;
   max.f = MAX_RGB9E5;

   if (f.u > 0x7f800000)
  /* catches neg, NaNs */
      return 0;
   else if (f.u >= max.u)
      return max.u;
   else
      return f.u;
}

static uint32_t float3_to_rgb9e5(const float rgb[3])
{
   int rm, gm, bm, exp_shared;
   uint32_t revdenom_biasedexp;
   union { float f; uint32_t u; } rc, bc, gc, maxrgb, revdenom;

   rc.u = rgb9e5_ClampRange(rgb[0]);
   gc.u = rgb9e5_ClampRange(rgb[1]);
   bc.u = rgb9e5_ClampRange(rgb[2]);
   maxrgb.u = MAX3(rc.u, gc.u, bc.u);

   /*
    * Compared to what the spec suggests, instead of conditionally adjusting
    * the exponent after the fact do it here by doing the equivalent of +0.5 -
    * the int add will spill over into the exponent in this case.
    */
   maxrgb.u += maxrgb.u & (1 << (23-9));
   exp_shared = MAX2((maxrgb.u >> 23), -RGB9E5_EXP_BIAS - 1 + 127) +
                1 + RGB9E5_EXP_BIAS - 127;
   revdenom_biasedexp = 127 - (exp_shared - RGB9E5_EXP_BIAS -
                               RGB9E5_MANTISSA_BITS) + 1;
   revdenom.u = revdenom_biasedexp << 23;
   assert(exp_shared <= RGB9E5_MAX_VALID_BIASED_EXP);

   /*
    * The spec uses strict round-up behavior (d3d10 disagrees, but in any case
    * must match what is done above for figuring out exponent).
    * We avoid the doubles ((int) rc * revdenom + 0.5) by doing the rounding
    * ourselves (revdenom was adjusted by +1, above).
    */
   rm = (int) (rc.f * revdenom.f);
   gm = (int) (gc.f * revdenom.f);
   bm = (int) (bc.f * revdenom.f);
   rm = (rm & 1) + (rm >> 1);
   gm = (gm & 1) + (gm >> 1);
   bm = (bm & 1) + (bm >> 1);

   assert(rm <= MAX_RGB9E5_MANTISSA);
   assert(gm <= MAX_RGB9E5_MANTISSA);
   assert(bm <= MAX_RGB9E5_MANTISSA);
   assert(rm >= 0);
   assert(gm >= 0);
   assert(bm >= 0);

   return (exp_shared << 27) | (bm << 18) | (gm << 9) | rm;
}

static void rgb9e5_to_float3(uint32_t rgb, float retval[3])
{
   int exponent;
   union { float f; uint32_t u; } scale;

   exponent = (rgb >> 27) - RGB9E5_EXP_BIAS - RGB9E5_MANTISSA_BITS;
   scale.u = (exponent + 127) << 23;

   retval[0] = ( rgb        & 0x1ff) * scale.f;
   retval[1] = ((rgb >> 9)  & 0x1ff) * scale.f;
   retval[2] = ((rgb >> 18) & 0x1ff) * scale.f;
}

/* End source */

typedef struct _function {
    instruction_type type;
    bool (*func)(emu_state* state, instruction* instruction);
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

static bool get_value(emu_state* state, operation_src src, int64_t* value)
{
    return get_value_(state, src, value, 0);
}

static bool put_value_(emu_state* state, operation_src src, int64_t value, int offset)
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

static bool put_value(emu_state* state, operation_src src, int64_t value)
{
    return put_value_(state, src, value, 0);
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

typedef int64_t (*loadstore_processor)(int64_t value, bool isload, int index);

static bool emulate_data_loadstore_helper(emu_state* state, instruction_data_load_store instr, bool isload, int type_size, loadstore_processor proc, bool issigned)
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
                    value = proc(value, true, i);
                }
                put_value_(state, instr.memory_reg, value, pos_reg);
            }
            else
            {
                int64_t value;
                check(get_value_(state, instr.memory_reg, &value, pos_reg));
                if (proc)
                {
                    value = proc(value, false, i);
                }
                set_buffer(state, offset + i, type_size, value);
            }
            pos_reg++;
        }
    }
    return true;
}

static bool emulate_data_loadstore_helper_packed(emu_state* state, instruction_data_load_store instr, bool isload, loadstore_processor proc, int rounds)
{
    int64_t offset;
    check(get_value(state, instr.memory_offset, &offset));

    if (isload)
    {
        int64_t value = get_buffer(state, offset / 4, 4, false);
        for (int i = 0; i < rounds; i++)
        {
            int64_t value2 = proc(value, true, i);
            put_value_(state, instr.memory_reg, value2, i);
        }
    }
    else
    {
        int64_t value = 0;
        for (int i = 0; i < rounds; i++)
        {
            int64_t value2;
            check(get_value_(state, instr.memory_reg, &value2, i));
            value2 = proc(value2, false, i);
            value |= value2;
        }
        set_buffer(state, offset / 4, 4, value);
    }
    return true;
}

static int64_t loadstore_processor_u8norm(int64_t value, bool isload, int index)
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

static int64_t loadstore_processor_s8norm(int64_t value, bool isload, int index)
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

static int64_t loadstore_processor_u16norm(int64_t value, bool isload, int index)
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

static int64_t loadstore_processor_s16norm(int64_t value, bool isload, int index)
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

static int64_t bitextract(int64_t value, int64_t mask, int shift, bool isload)
{
    if (isload)
    {
        int64_t temp = (value >> shift) & mask;
        return float32_to_int((float)temp / mask);
    }
    else
    {
        int64_t temp = int_to_float32(value) * mask;
        return (temp & mask) << shift;
    }
}

static int64_t loadstore_processor_rgb10a2(int64_t value, bool isload, int index)
{
    assert(index >= 0 && index <= 3);
    
    switch (index)
    {
        case 0: // R
            return bitextract(value, 0x3FF, 0, isload);
        case 1: // G
            return bitextract(value, 0x3FF, 10, isload);
        case 2: // B
            return bitextract(value, 0x3FF, 20, isload);
        case 3: // A
            return bitextract(value, 0x3, 30, isload);
        default:
            assert(0);
    }
}

static int64_t loadstore_processor_rgb11b10f(int64_t value, bool isload, int index)
{
    assert(index >= 0 && index <= 2);
    
    flexfloat_t floaty;
    flexfloat_desc_t desc = {5, index == 2 ? 5 : 6};
    
    if (isload)
    {
        uint32_t extracted = 0;
        ff_init(&floaty, desc);
        switch (index)
        {
            case 0: // R
                extracted = value & 0x7FF;
                break;
            case 1: // G
                extracted = (value >> 11) & 0x7FF;
                break;
            case 2: // B
                extracted = (value >> 22) & 0x3FF;
                break;
        }
        flexfloat_set_bits(&floaty, extracted);
        float ret = ff_get_float(&floaty);
        return float32_to_int(ret);
    }
    else
    {
        ff_init_float(&floaty, int_to_float32(value), desc);
        uint32_t bits = flexfloat_get_bits(&floaty) + 1;
        switch (index)
        {
            case 0: // R
                return bits & 0x7FF;
            case 1: // G
                return (bits & 0x7FF) << 11;
            case 2: // B
                return (bits & 0x3FF) << 22;
        }
    }
    assert(0);
}

static int64_t loadstore_processor_srgb8(int64_t value, bool isload, int index)
{
    if (index == 3)
    {
        return loadstore_processor_u8norm(value, isload, index);
    }
    
    if (isload)
    {
        return float32_to_int(util_format_srgb_to_linear_float((float)value / 0xFF));
    }
    else
    {
        return util_format_linear_to_srgb_float(int_to_float32(value)) * 0xFF;
    }
}

static bool emulate_data_loadstore(emu_state* state, instruction* instruction, bool isload)
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
        case FORMAT_RGB10A2:
            check(emulate_data_loadstore_helper_packed(state, instr, isload, loadstore_processor_rgb10a2, 4));
            break;
        case FORMAT_RG11B10F:
            check(emulate_data_loadstore_helper_packed(state, instr, isload, loadstore_processor_rgb11b10f, 3));
            break;
        case FORMAT_SRGBA8:
            check(emulate_data_loadstore_helper(state, instr, isload, 1, loadstore_processor_srgb8, false));
            break;
        case FORMAT_RGB9E5:
            {
                int64_t offset;
                check(get_value(state, instr.memory_offset, &offset));
                
                if (isload)
                {
                    int64_t value = get_buffer(state, offset / 4, 4, false);
                    float ret[3];
                    rgb9e5_to_float3((uint32_t)value, ret);
                    put_value_(state, instr.memory_reg, float32_to_int(ret[0]), 0);
                    put_value_(state, instr.memory_reg, float32_to_int(ret[1]), 1);
                    put_value_(state, instr.memory_reg, float32_to_int(ret[2]), 2);
                }
                else
                {
                    int64_t valueR, valueG, valueB;
                    check(get_value_(state, instr.memory_reg, &valueR, 0));
                    check(get_value_(state, instr.memory_reg, &valueG, 1));
                    check(get_value_(state, instr.memory_reg, &valueB, 2));
                    
                    float floats[3] = { int_to_float32(valueR), int_to_float32(valueG), int_to_float32(valueB) };
                    uint32_t ret = float3_to_rgb9e5(floats);

                    set_buffer(state, offset / 4, 4, ret);
                }
            }
            break;
        default:
            error("Unhandled format %d", instr.format);
    }
    
    return true;
}

static bool emulate_data_store(emu_state* state, instruction* instruction)
{
    return emulate_data_loadstore(state, instruction, false);
}

static bool emulate_data_load(emu_state* state, instruction* instruction)
{
    return emulate_data_loadstore(state, instruction, true);
}

static bool emulate_mov(emu_state* state, instruction* instruction)
{
    instruction_mov instr = instruction->data.mov;
    int64_t value;
    check(get_value(state, instr.source, &value));
    check(put_value(state, instr.dest, value));
    return true;
}

static bool emulate_wait(emu_state* state, instruction* instruction)
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

static bool call_func(emu_state* state, instruction* instruction)
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

bool emulate_instructions(emu_state* state, instruction* instructions)
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
