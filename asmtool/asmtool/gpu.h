#ifndef gpu_h
#define gpu_h

#include "helper.h"

/* hardware */

bool get_results_from_gpu(binary_data code);
void hardware_init(void);
bool compile_shader_to_metallib(char* sourcecode, binary_data* data);

/* metallib */

bool get_bytecode_from_metallib(binary_data data, binary_data* data_out);

/* disassembler */

void disassemble_bytecode_to_structs(binary_data data);

/* assembler */


/* instructions */

enum instruction_type {
    INSTRUCTION_TYPE_IADD,
    INSTRUCTION_TYPE_IMUL,
    //...
};
struct instruction_data_iadd {
    int source;
    int dest;
    //...
};
struct instruction_data_imul {
    int source;
    int dest;
    //...
};
union instruction_data
{
    struct instruction_data_iadd iadd;
    struct instruction_data_imul imul;
};
struct instruction {
    enum instruction_type type;
    union instruction_data data;
};

#endif /* gpu_h */
