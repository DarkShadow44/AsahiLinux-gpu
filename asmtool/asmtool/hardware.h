#ifndef hardware_h
#define hardware_h

#include "helper.h"

bool get_results_from_gpu(binary_data code);
void hardware_init(void);
bool compile_shader_to_metallib(char* sourcecode, binary_data* data);

#endif /* hardware_h */
