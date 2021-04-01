#include "gpu.h"

static void concat_arguments(binary_data* data, int argc, const char* argv[], int start)
{
    assert(data->data == 0);
    
    int len = 0;
    
    for (int i = start; i < argc; i++)
    {
        len += strlen(argv[i]) + 1; // +1 for space
    }
    
    data->data = calloc(len, 1);
    data->len = len;
    
    for (int i = start; i < argc; i++)
    {
        strcat((char*)data->data, " ");
        strcat((char*)data->data, argv[i]);
    }
}

static bool handle_command(int argc, const char* argv[])
{
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
                read_file(path, &data_metal, true);
                compile_shader_to_metallib((char*)data_metal.data, &data_metallib);
                get_bytecode_from_metallib(data_metallib, &data_bytecode);
                write_file("/Users/fabian/Programming/GPU/applegpu/out.dat", data_bytecode);
                // dump_to_binary(data_bytecode.data + 6, 8);
                // dump_to_hex(data_bytecode.data, data_bytecode.len);
                // disassemble_bytecode_to_structs(data_bytecode);
                // test_output out;
                // get_results_from_gpu(data_bytecode, &out);
                // get_results_from_gpu(data_bytecode, 0);
                
                dump_disassembly(data_bytecode);
                
                free(data_metallib.data);
                free(data_metal.data);
                free(data_bytecode.data);
            }
        }
        if (!strcmp(command, "asm"))
        {
            binary_data data_text = {0};
            concat_arguments(&data_text, argc, argv, 2);
            
            binary_data data_bytecode = {0};
            assemble_text_to_bytecode(data_text, &data_bytecode);
            dump_disassembly(data_bytecode);
            
            free(data_text.data);
            free(data_bytecode.data);
        }
        if (!strcmp(command, "disasm"))
        {
            if (argc > 2)
            {
                const char* path = argv[2];
                
                binary_data data_bytecode = {0};
                check(read_file(path, &data_bytecode, false));
                
                instruction* instructions;
                check(disassemble_bytecode_to_structs(data_bytecode, &instructions));
                
                binary_data data_disassembly = {0};
                check(disassemble_structs_to_text(instructions, &data_disassembly, true));
                printf("%s\n", data_disassembly.data);
                
                instruction* instructions_asm;
                check(assemble_text_to_structs(data_disassembly, &instructions_asm));
            }
        }
        if (!strcmp(command, "test"))
        {
            check(run_tests());
        }
    }
    return true;
}

int main(int argc, const char* argv[]) {
    
    hardware_init();
    
    const char* params_dump[] = {
        "",
        "dump_metallib",
        "/Users/fabian/Programming/AsahiLinux-gpu/asmtool/testing/test.metal"
    };
    
    const char* params_disasm[] = {
        "",
        "disasm",
        "/Users/fabian/Programming/GPU/applegpu/out.dat"
    };
    
    const char* params_asm[] = {
        "",
        "asm",
        "device_load i32, 0xF, r6_r7_r8_r9, r17_r18, 4, signed"
    };
    
    const char* params_test[] = {
        "",
        "test",
        ""
    };
    
    (void)params_disasm;
    (void)params_dump;
    (void)params_test;
    (void)params_asm;
    
    if (argc == 1)
    {
        argc = 3;
        argv = params_asm;
    }
    
    if (!handle_command(argc, argv))
    {
        return 1;
    }
    return 0;
}
