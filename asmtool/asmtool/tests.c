#include "gpu.h"

static bool _run_test(unsigned char* data, int len, uint32_t* output_expected, int output_expected_len, const char* file, int line)
{
    binary_data bytecode;
    bytecode.data = data;
    bytecode.len = len;
    
    test_output output;
    
    validate(output_expected_len == 256, "Output length is %d\n", output_expected_len);
    
    check(get_results_from_gpu(bytecode, &output));
    
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            int pos = i + j * 16;
            if (output_expected[pos] != output.buffer0[i][j])
            {
                _error(file, line, "At %d: Expected %X, got %X\n", pos, output_expected[pos], output.buffer0[i][j]);
            }
        }
    }
    
    return true;
}
#define run_test(data, output_expected) \
    _run_test(data, sizeof(data), output_expected, sizeof(output_expected), __FILE__, __LINE__)

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
    
    check(run_test(test1, test1_output));
    
    return true;
}
bool run_tests(void)
{
    check(test_basic());
    return true;
}
