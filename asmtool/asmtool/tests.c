#include "gpu.h"

static void copy_input(test_io* io, uint32_t* input)
{
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            int pos = i + j * 16;
            io->buffer0[i][j] = input[pos];
        }
    }
}

static bool _run_test(unsigned char* data, int len, uint32_t* output_expected, int output_expected_len, uint32_t* input, const char* file, int line)
{
    binary_data bytecode;
    bytecode.data = data;
    bytecode.len = len;
    
    test_io io_gpu = {0};
    copy_input(&io_gpu, input);
    
    validate(output_expected_len == 256, "Output length is %d\n", output_expected_len);
    
    instruction* instructions;
    binary_data data_bytecode;
    data_bytecode.data = data;
    data_bytecode.len = len;
    check(disassemble_bytecode_to_structs(data_bytecode, &instructions));
    
    binary_data data_disassembly = {0};
    check(disassemble_structs_to_text(instructions, &data_disassembly, false));
    
    check(get_results_from_gpu(bytecode, &io_gpu));
    
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            int pos = i + j * 16;
            if (output_expected[pos] != io_gpu.buffer0[i][j])
            {
                error_(file, line, "GPU test at %d: Expected %X, got %X\n", pos, output_expected[pos], io_gpu.buffer0[i][j]);
            }
        }
    }
    
    emu_state emu_state = {0};
    copy_input(&emu_state.data, input);
    check(emulate_instructions(&emu_state, instructions));
    
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            int pos = i + j * 16;
            if (output_expected[pos] != emu_state.data.buffer0[i][j])
            {
                error_(file, line, "Emu test at %d: Expected %X, got %X\n", pos, output_expected[pos], emu_state.data.buffer0[i][j]);
            }
        }
    }

    instruction* instructions_asm;
    check(assemble_text_to_structs(data_disassembly, &instructions_asm));
    
    binary_data data_assembly = {0};
    check(assemble_structs_to_bytecode(instructions_asm, &data_assembly));

#define DUMP 0
    if (DUMP)
    {
        dump_disassembly(data_bytecode);
        dump_disassembly(data_assembly);
    }
#undef DUMP
    
    validate_(file, line, data_assembly.len == data_bytecode.len, "Expected len of %d, got %d", data_bytecode.len, data_assembly.len);
    for (int i = 0; i < data_bytecode.len; i++)
    {
        if (data_bytecode.data[i] != data_assembly.data[i])
        {
            error_(file, line, "Asm test at %X: Expected %X, got %X\n", i, data_bytecode.data[i], data_assembly.data[i]);
        }
    }
    
    destroy_instruction_list(instructions);
    destroy_instruction_list(instructions_asm);
    free(data_disassembly.data);
    free(data_assembly.data);
    
    return true;
}
#define run_test(data, output_expected, input) \
    _run_test(data, sizeof(data), output_expected, sizeof(output_expected), input, __FILE__, __LINE__)

static uint32_t input_zero[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static bool test_basic(void)
{
    uint32_t test1_output[] =
    {
        0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    unsigned char test1[] = {
        0x62, 0x19, 0x01, 0x00, 0x00, 0x00,             // mov r6, 1;
        0x62, 0x1D, 0x02, 0x00, 0x00, 0x00,             // mov r7, 2;
        0x62, 0x21, 0x03, 0x00, 0x00, 0x00,             // mov r8, 3;
        0x62, 0x25, 0x04, 0x00, 0x00, 0x00,             // mov r9, 4;
        0x45, 0x31, 0x12, 0x05, 0x20, 0xC8, 0xF2, 0x00, // device_store i32, 0xF, r6_r7_r8_r9, r17_r18, 4, signed;
        0x88, 0x00                                      // stop
    };
    
    check(run_test(test1, test1_output, input_zero));
    
    return true;
}

static bool test_mov(void)
{
    uint32_t test1_output[] =
    {
        0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 100000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    uint32_t test2_output[] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x10002, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    unsigned char test1[] = {
        0x62, 0x19, 0x01, 0x00, 0x00, 0x00,             // mov r6, 1;
        0x62, 0x1D, 0x02, 0x00, 0x00, 0x00,             // mov r7, 2;
        0x62, 0x21, 0xA0, 0x86, 0x01, 0x00,             // mov r8, 100000;
        0x62, 0x25, 0x04, 0x00, 0x00, 0x00,             // mov r9, 4;
        0x45, 0x31, 0x10, 0x0D, 0x00, 0xC8, 0xF2, 0x00, // device_store i32, 0xF, r6_r7_r8_r9, u0_u1, 4, signed;
        0x88, 0x00                                      // stop
    };
    
    unsigned char test2[] = {
        0x62, 0x19, 0x00, 0x00, 0x00, 0x00,             // mov r6, 0;
        0x62, 0x1D, 0x00, 0x00, 0x00, 0x00,             // mov r7, 0;
        0x62, 0x21, 0x00, 0x00, 0x00, 0x00,             // mov r8, 0;
        0x62, 0x26, 0x01, 0x00,                         // mov r9h, 1;
        0x62, 0x24, 0x02, 0x00,                         // mov r9l, 2;
        0x45, 0x31, 0x10, 0x0D, 0x00, 0xC8, 0xF2, 0x00, // device_store i32, 0xF, r6_r7_r8_r9, u0_u1, 4, signed;
        0x88, 0x00                                      // stop
    };
    
    unsigned char test3[] = {
        0x62, 0x89, 0x01, 0x00, 0x00, 0x00, 0x00, 0x10, // mov r34, 1;
        0x62, 0x8D, 0x02, 0x00, 0x00, 0x00, 0x00, 0x10, // mov r35, 2;
        0x62, 0x91, 0xA0, 0x86, 0x01, 0x00, 0x00, 0x10, // mov r36, 100000;
        0x62, 0x95, 0x04, 0x00, 0x00, 0x00, 0x00, 0x10, // mov r37, 4;
        0x45, 0x11, 0x10, 0x0D, 0x00, 0xC9, 0xF2, 0x00, // device_store i32, 0xF, r34_r35_r36_r37, u0_u1, 4, signed;
        0x88, 0x00                                      // stop
    };
    
    unsigned char test4[] = {
        0x62, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, // mov r34, 0;
        0x62, 0x8D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, // mov r35, 0;
        0x62, 0x91, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, // mov r36, 0;
        0x62, 0x96, 0x01, 0x00, 0x00, 0x10,             // mov r37h, 1;
        0x62, 0x94, 0x02, 0x00, 0x00, 0x10,             // mov r37l, 2;
        0x45, 0x11, 0x10, 0x0D, 0x00, 0xC9, 0xF2, 0x00, // device_store i32, 0xF, r34_r35_r36_r37, u0_u1, 4, signed;
        0x88, 0x00                                      // stop
    };
    
    
     check(run_test(test1, test1_output, input_zero)); /* Test mov with 32bit register */
     check(run_test(test2, test2_output, input_zero)); /* Test mov with 16bit register */
     check(run_test(test3, test1_output, input_zero)); /* Test mov with 32bit register and long flag */
     check(run_test(test4, test2_output, input_zero)); /* Test mov with 16bit register and long flag */
    
    return true;
}

static bool test_load_store(void)
{
    uint32_t test1_input[] =
    {
        0, 0x1020, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x3040, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x5060, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x7080, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    uint32_t test5_input[] =
    {
        0xF1F2F3F4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xF5F6F7F8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xF9E0E1E2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xE3E4E5E6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    uint32_t test1_output[] =
    {
        0, 0x1020, 0x1020, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x3040, 0x3040, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x5060, 0x5060, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x7080, 0x7080, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    uint32_t test2_output[] =
    {
        0, 0x1020,      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x3040, 0x1020, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x5060, 0x3040, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x7080, 0x5060, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    uint32_t test3_output[] =
    {
        0, 0x1020,      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x3040,      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x5060,      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x7080, 0x1020, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    uint32_t test5_output[] =
    {
        0xF1F2F3F4, 0xF5F600F8, 0, 0xF8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xF5F6F7F8,          0, 0,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xF9E0E1E2,          0, 0, 0xF6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xE3E4E5E6,          0, 0, 0xF5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    uint32_t test6_output[] =
    {
        0xF1F2F3F4, 0x0000E1E2, 0, 0xE1E2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xF5F6F7F8, 0xE3E4E5E6, 0,      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xF9E0E1E2, 0,          0, 0xE5E6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xE3E4E5E6, 0,          0, 0xE3E4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    uint32_t test7_output[] =
    {
        0xF1F2F3F4, 0xF5F600F8, 0, 0x3F78F8F9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xF5F6F7F8,          0, 0,          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xF9E0E1E2,          0, 0, 0x3F76F6F7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xE3E4E5E6,          0, 0, 0x3F75F5F6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    uint32_t test8_output[] =
    {
        0xF1F2F3F4, 0xF5F600F8, 0, 0xBD810204, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xF5F6F7F8,          0, 0,          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xF9E0E1E2,          0, 0, 0xBDA14285, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xE3E4E5E6,          0, 0, 0xBDB162C6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    uint32_t test9_output[] =
    {
        0xF1F2F3F4, 0x0000E1E2, 0, 0x3F61E2E2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xF5F6F7F8, 0xE3E4E5E6, 0,          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xF9E0E1E2,          0, 0, 0x3F65E6E6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xE3E4E5E6,          0, 0, 0x3F63E4E4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    uint32_t test10_output[] =
    {
        0xF1F2F3F4, 0x0000E1E2, 0, 0xBE70F1E2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xF5F6F7F8, 0xE3E4E5E6, 0,          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xF9E0E1E2,          0, 0, 0xBE50D1A2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xE3E4E5E6,          0, 0, 0xBE60E1C2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    unsigned char test1[] = {
        0x05, 0x31, 0x10, 0x0D, 0x00, 0xC8, 0xF2, 0x00, // device_load  i32, 0xF, r6_r7_r8_r9, u0_u1, 4, signed;
        0x38, 0x00,                                     // wait
        0x45, 0x31, 0x20, 0x0D, 0x00, 0xC8, 0xF2, 0x00, // device_store i32, 0xF, r6_r7_r8_r9, u0_u1, 8, signed;
        0x88, 0x00                                      // stop
    };
    
    unsigned char test2[] = {
        0x05, 0x31, 0x10, 0x0D, 0x00, 0xC8, 0x72, 0x00, // device_load  i32, 0x7, r6_r7_r8, u0_u1, 4, signed;
        0x38, 0x00,                                     // wait
        0x45, 0x31, 0x20, 0x0D, 0x00, 0xC8, 0xE2, 0x00, // device_store i32, 0xE, r6_r7_r8, u0_u1, 8, signed;
        0x88, 0x00                                      // stop
    };
    
    unsigned char test3[] = {
        0x05, 0x31, 0x10, 0x0D, 0x00, 0xC8, 0x12, 0x00, // device_load  i32, 0x1, r6, u0_u1, 4, signed;
        0x38, 0x00,                                     // wait
        0x45, 0x31, 0x20, 0x0D, 0x00, 0xC8, 0x82, 0x00, // device_store i32, 0x8, r6, u0_u1, 8, signed;
        0x88, 0x00                                      // stop
    };
    
    unsigned char test4[] = {
        0x05, 0x31, 0x10, 0x0D, 0x00, 0xC8, 0x02, 0x00, // device_load  i32, 0x0, r6, u0_u1, 4, signed;
        0x38, 0x00,                                     // wait
        0x45, 0x31, 0x20, 0x0D, 0x00, 0xC8, 0x02, 0x00, // device_store i32, 0x0, r6, u0_u1, 8, signed;
        0x88, 0x00                                      // stop
    };
    
    unsigned char test5[] = {
        0x05, 0x30, 0x10, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_load   i8, 0xD, r6_r7_r8_r9, u0_u1,  4, signed;
        0x38, 0x00,                                     // wait
        0x45, 0x31, 0x30, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_store i32, 0xD, r6_r7_r8_r9, u0_u1, 12, signed;
        0x45, 0x30, 0x40, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_store  i8, 0xD, r6_r7_r8_r9, u0_u1, 16, signed;
        0x88, 0x00                                      // stop
    };
    
    unsigned char test6[] = {
        0x85, 0x30, 0x10, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_load  i16, 0xD, r6_r7_r8_r9, u0_u1,  4, signed;
        0x38, 0x00,                                     // wait
        0x45, 0x31, 0x30, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_store i32, 0xD, r6_r7_r8_r9, u0_u1, 12, signed;
        0xC5, 0x30, 0x20, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_store i16, 0xD, r6_r7_r8_r9, u0_u1,  8, signed;
        0x88, 0x00                                      // stop
    };
    
    unsigned char test7[] = {
        0x05, 0x32, 0x10, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_load  u8norm, 0xD, r6_r7_r8_r9, u0_u1,  4, signed;
        0x38, 0x00,                                     // wait
        0x45, 0x31, 0x30, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_store    i32, 0xD, r6_r7_r8_r9, u0_u1, 12, signed;
        0x45, 0x32, 0x40, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_store u8norm, 0xD, r6_r7_r8_r9, u0_u1, 16, signed;
        0x88, 0x00                                      // stop
    };
    
    unsigned char test8[] = {
        0x85, 0x32, 0x10, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_load  s8norm, 0xD, r6_r7_r8_r9, u0_u1,  4, signed;
        0x38, 0x00,                                     // wait
        0x45, 0x31, 0x30, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_store    i32, 0xD, r6_r7_r8_r9, u0_u1, 12, signed;
        0xC5, 0x32, 0x40, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_store s8norm, 0xD, r6_r7_r8_r9, u0_u1, 16, signed;
        0x88, 0x00                                      // stop
    };
    
    unsigned char test9[] = {
        0x05, 0x33, 0x10, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_load  u16norm, 0xD, r6_r7_r8_r9, u0_u1,  4, signed;
        0x38, 0x00,                                     // wait
        0x45, 0x31, 0x30, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_store     i32, 0xD, r6_r7_r8_r9, u0_u1, 12, signed;
        0x45, 0x33, 0x20, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_store u16norm, 0xD, r6_r7_r8_r9, u0_u1, 8, signed;
        0x88, 0x00                                      // stop
    };
    
    unsigned char test10[] = {
        0x85, 0x33, 0x10, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_load  s16norm, 0xD, r6_r7_r8_r9, u0_u1,  4, signed;
        0x38, 0x00,                                     // wait
        0x45, 0x31, 0x30, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_store     i32, 0xD, r6_r7_r8_r9, u0_u1, 12, signed;
        0xC5, 0x33, 0x20, 0x0D, 0x00, 0xC8, 0xD2, 0x00, // device_store s16norm, 0xD, r6_r7_r8_r9, u0_u1, 8, signed;
        0x88, 0x00                                      // stop
    };
    
    check(run_test(test1, test1_output, test1_input)); /* Simple load/store */
    check(run_test(test2, test2_output, test1_input)); /* Mask 0111 -> 1110 */
    check(run_test(test3, test3_output, test1_input)); /* Mask 0001 -> 1000 */
    check(run_test(test4, test1_input, test1_input));  /* Mask 0000 -> 0000 */
    check(run_test(test5, test5_output, test5_input)); /* Mask 1101, format i8 */
    check(run_test(test6, test6_output, test5_input)); /* Mask 1101, format i16 */
    check(run_test(test7, test7_output, test5_input)); /* Mask 1101, format unorm8 */
    check(run_test(test8, test8_output, test5_input)); /* Mask 1101, format s8norm */
    check(run_test(test9, test9_output, test5_input)); /* Mask 1101, format u16norm */
    check(run_test(test10, test10_output, test5_input)); /* Mask 1101, format s16norm */
    
    return true;
}

bool run_tests(void)
{
    check(test_basic());
    check(test_mov());
    check(test_load_store());
    printf("Tests finished!\n");
    return true;
}
