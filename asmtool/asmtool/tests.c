#include "gpu.h"

static void _run_test(unsigned char* data, int len, test_output* output)
{
    binary_data bytecode;
    bytecode.data = data;
    bytecode.len = len;
    
    get_results_from_gpu(bytecode, output);
}
#define run_test(data) \
    _run_test(data, sizeof(data), &output)

static void test_mov(void)
{
    test_output output;
    
    unsigned char mov1[] = {};
    
    run_test(mov1);
  //  assert(output)
}
void run_tests(void)
{
    test_mov();
}
