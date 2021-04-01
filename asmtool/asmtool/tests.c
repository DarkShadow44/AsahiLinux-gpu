#include "gpu.h"

static bool _run_test(unsigned char* data, int len, uint32_t* output_expected, int output_expected_len, uint32_t* input, const char* file, int line)
{
    binary_data bytecode;
    bytecode.data = data;
    bytecode.len = len;
    
    test_io io_gpu = {0};
    memcpy(io_gpu.input0, input, 256);
    
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
            if (output_expected[pos] != io_gpu.output0[i][j])
            {
                error_(file, line, "GPU test at %d: Expected %X, got %X\n", pos, output_expected[pos], io_gpu.output0[i][j]);
            }
        }
    }
    
    emu_state emu_state = {0};
    memcpy(emu_state.data.input0, input, 256);
    check(emulate_instructions(&emu_state, instructions));
    
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            int pos = i + j * 16;
            if (output_expected[pos] != emu_state.data.output0[i][j])
            {
                error_(file, line, "Emu test at %d: Expected %X, got %X\n", pos, output_expected[pos], emu_state.data.output0[i][j]);
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
    uint32_t test1_output[] =
    {
        0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 100000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    
    unsigned char test1[] = {
        0x62, 0x19, 0x01, 0x00, 0x00, 0x00,             // mov r6, 1;
        0x62, 0x1D, 0x02, 0x00, 0x00, 0x00,             // mov r7, 2;
        0x62, 0x21, 0xA0, 0x86, 0x01, 0x00,             // mov r8, 100000;
        0x62, 0x25, 0x04, 0x00, 0x00, 0x00,             // mov r9, 4;
        0x45, 0x31, 0x10, 0x0D, 0x00, 0xC8, 0xF2, 0x00, // device_store i32, 0xF, r6_r7_r8_r9, u0_u1, 4, signed;
        0x88, 0x00                                      // stop
    };
    
     check(run_test(test1, test1_output, input_zero)); /*  */
    
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
