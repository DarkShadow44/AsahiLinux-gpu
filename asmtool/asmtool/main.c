#include "gpu.h"

int main(int argc, const char* argv[]) {
    
    hardware_init();
    
    const char* params[] = {
        "",
        "dump_metallib",
        "/Users/fabian/Programming/AsahiLinux-gpu/asmtool/testing/test.metal"
    };
    argc = 3;
    argv = params;
    
    if (argc > 1)
    {
        const char* command = argv[1];
        
        if (!strcmp(command, "dump_metallib"))
        {
            if (argc > 2)
            {
                const char* path = argv[2];
                binary_data data_metal = {0};
                binary_data data_bytecode = {0};
                binary_data data_metallib = {0};
                read_file(path, &data_metal);
                compile_shader_to_metallib((char*)data_metal.data, &data_metallib);
                get_bytecode_from_metallib(data_metallib, &data_bytecode);
                write_file("/Users/fabian/Programming/GPU/applegpu/out.dat", data_bytecode);
                // dump_to_binary(data_bytecode.data + 6, 8);
                // dump_to_hex(data_bytecode.data, data_bytecode.len);
                // disassemble_bytecode_to_structs(data_bytecode);
                
                get_results_from_gpu(data_bytecode);
                get_results_from_gpu(data_bytecode);
                
                free(data_metallib.data);
                free(data_metal.data);
                free(data_bytecode.data);
            }
        }
        return 0;
    }
    return 0;
}
