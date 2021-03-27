#ifndef gpu_h
#define gpu_h

#include "helper.h"

/* tests */

#define TEST_BUFFER_COUNT 1
#define TEST_BUFFER_SIZE 16

bool run_tests(void);

typedef struct _test_output
{
    uint32_t buffer0[TEST_BUFFER_SIZE][4];
} test_output;


/* hardware */

bool get_results_from_gpu(binary_data code, test_output* output);
void hardware_init(void);
bool compile_shader_to_metallib(char* sourcecode, binary_data* data);

/* metallib */

bool get_bytecode_from_metallib(binary_data data, binary_data* data_out);


/* instructions */

typedef enum _instruction_type {
    INSTRUCTION_LOAD,
    INSTRUCTION_STORE,
    INSTRUCTION_MOV,
    INSTRUCTION_IADD,
    INSTRUCTION_IMUL,
    INSTRUCTION_RET,
    //...
} instruction_type;

typedef enum _operation_src_type
{
    OPERATION_SOURCE_IMMEDIATE,
    OPERATION_SOURCE_REG16,
    OPERATION_SOURCE_REG32,
    OPERATION_SOURCE_REG64,
    OPERATION_SOURCE_UNIFORM64,
} operation_src_type;

typedef enum _operation_src_flags
{
    OPERATION_FLAG_SIGN_EXTEND = 1 << 0,
} operation_src_flags;

typedef struct _operation_src
{
    operation_src_type type;
    operation_src_flags flags;
    int value;
} operation_src;

typedef struct _instruction_data_iadd {
    int source;
    int dest;
    //...
} instruction_data_iadd;

typedef struct _instruction_data_imul {
    int source;
    int dest;
    //...
} instruction_data_imul;

typedef struct _instruction_data_load_store {
    operation_src memory_offset;
    operation_src memory_base;
    operation_src memory_reg;
    int format;
    int mask;
} instruction_data_load_store;

typedef struct _instruction_ret {
    int reg;
} instruction_ret;

typedef struct _instruction_mov {
    int reg;
    int value;
} instruction_mov;

typedef union _instruction_data
{
    instruction_data_iadd iadd;
    instruction_data_imul imul;
    instruction_data_load_store load_store;
    instruction_ret ret;
    instruction_mov mov;
} instruction_data;

typedef struct _instruction {
    unsigned char original_bytes[20];
    int original_len;
    struct _instruction* next;
    instruction_type type;
    instruction_data data;
} instruction;

static inline instruction* init_instruction()
{
    instruction* ret = malloc(sizeof(instruction));
    memset(ret, 0, sizeof(instruction));
    return ret;
}

static inline void destroy_instruction_list(instruction* list)
{
    while (list)
    {
        instruction* temp = list->next;
        free(list);
        list = temp;
    }
}


/* disassembler */

bool disassemble_bytecode_to_structs(binary_data bytecode, instruction** instructions);
bool disassemble_structs_to_text(instruction* instructions, binary_data* text, bool print_offsets);


/* assembler */

bool assemble_text_to_structs(binary_data text, instruction** instructions);
bool assemble_structs_to_bytecode(instruction* instructions, binary_data* bytecode);

#endif /* gpu_h */
