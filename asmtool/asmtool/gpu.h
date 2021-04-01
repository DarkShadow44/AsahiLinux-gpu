#ifndef gpu_h
#define gpu_h

/* helper */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct _binary_data
{
    unsigned char* data;
    unsigned int len;
} binary_data;

bool binary_data_sub(binary_data original, binary_data* data_out, int start, int len);

bool read_file(const char* path, binary_data* data, bool text);
bool write_file(const char* path, binary_data data);
int find_in_bytes(unsigned char* source, unsigned char* find, int source_len, int find_len);
void dump_to_binary(unsigned char* data, int len);
void dump_to_dec(unsigned char* data, int len);
void dump_to_hex(unsigned char* data, int len);

int count_bits(int value);

#define error_(file, line, err, ...) \
    { \
        char __errorbuffer[1000]; \
        sprintf(__errorbuffer, err, ##__VA_ARGS__); \
        printf("%s:%d: %s\n", file, line, __errorbuffer); \
        return false; \
    }

#define error(err, ...) \
    error_(__FILE__, __LINE__, err, ##__VA_ARGS__)

#define check(value) \
    if (!(value)) \
    { \
        return false; \
    }

#define validate_(file, line, value, err, ...) \
    if (!(value)) \
    { \
        error_(file, line, err, ##__VA_ARGS__) \
    }

#define validate(value, err, ...) \
    validate_(__FILE__, __LINE__, value, err, ##__VA_ARGS__)

#define ARRAY_SIZE(arr) \
  (sizeof(arr) / sizeof(arr[0]))

/* tests */

#define TEST_BUFFER_COUNT 1
#define TEST_BUFFER_SIZE 16

bool run_tests(void);

typedef struct _test_output
{
    uint32_t output0[TEST_BUFFER_SIZE][4];
    uint32_t input0[TEST_BUFFER_SIZE][4];
} test_io;

int64_t GET_BITS64(binary_data data, int start, int end);
int GET_BITS(binary_data data, int start, int end);
void SET_BITS(binary_data data, int start, int end, uint64_t value_new);


/* hardware */

bool get_results_from_gpu(binary_data code, test_io* io);
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
    INSTRUCTION_STOP,
} instruction_type;

typedef enum _operation_src_type
{
    OPERATION_SOURCE_IMMEDIATE,
    OPERATION_SOURCE_REG16L,
    OPERATION_SOURCE_REG16H,
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
    uint32_t value_int;
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
    operation_src source;
    operation_src dest;
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
bool disassemble_bytecode_to_text(binary_data data_bytecode, binary_data* data_text, bool print_offsets); /* helper */
void dump_disassembly(binary_data data_bytecode); /* helper */

/* assembler */

bool assemble_text_to_structs(binary_data text, instruction** instructions);
bool assemble_structs_to_bytecode(instruction* instructions, binary_data* bytecode);
bool assemble_text_to_bytecode(binary_data data_text, binary_data* data_bytecode);


/* emulator */

typedef struct _emu_state
{
    uint32_t reg[256];
    uint32_t uniform[256];
    test_io data;
} emu_state;

bool emulate_instructions(emu_state *state, instruction* instructions);

#endif /* gpu_h */
