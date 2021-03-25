#ifndef gpu_h
#define gpu_h

#include "helper.h"

/* tests */

#define TEST_BUFFER_COUNT 1
#define TEST_BUFFER_SIZE 100

void run_tests(void);

typedef struct _test_output
{
    uint32_t buffer0[100][4];
} test_output;


/* hardware */

bool get_results_from_gpu(binary_data code, test_output* output);
void hardware_init(void);
bool compile_shader_to_metallib(char* sourcecode, binary_data* data);

/* metallib */

bool get_bytecode_from_metallib(binary_data data, binary_data* data_out);


/* instructions */

enum instruction_type {
    INSTRUCTION_LOAD,
    INSTRUCTION_STORE,
    INSTRUCTION_MOV,
    INSTRUCTION_IADD,
    INSTRUCTION_IMUL,
    INSTRUCTION_RET,
    //...
};

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
    int reg;
} instruction_data_load_store;

typedef struct _instruction_ret {
    int reg;
} instruction_ret;

typedef struct _instruction_mov {
    int reg;
    int value;
} instruction_mov;

union instruction_data
{
    instruction_data_iadd iadd;
    instruction_data_imul imul;
    instruction_data_load_store load_store;
    instruction_ret ret;
    instruction_mov mov;
};

typedef struct _instruction {
    struct _instruction* next;
    enum instruction_type type;
    union instruction_data data;
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

bool disassemble_bytecode_to_structs(binary_data data, instruction** instructions);
bool disassemble_structs_to_text(instruction* instructions, binary_data* text);


/* assembler */



#endif /* gpu_h */
