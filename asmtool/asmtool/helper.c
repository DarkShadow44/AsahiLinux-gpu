#include "gpu.h"

#include <unistd.h>
#include <sys/fcntl.h>

int64_t GET_BITS64(binary_data data, int start, int end)
{
    if (end / 8 >= data.len)
    {
        assert(start / 8 >= data.len);
        return 0;
    }
    int start_full = start / 8;
    int start_part = start % 8;
    
    unsigned char* bytes = data.data + start_full;
    
    // Extract up to 7 bytes of data
    int64_t value = 0;
    for (int i = 0; i < 7 && i < data.len; i++)
    {
        value += (int64_t)bytes[i] << (i*8);
    }
    
    int64_t mask = ~(((int64_t)-1) << (end-start + 1));
    
    return (value >> start_part) & mask;
}

int GET_BITS(binary_data data, int start, int end)
{
    return (int)GET_BITS64(data, start, end);
}

void SET_BITS(binary_data data, int start, int end, uint64_t value_new)
{
    if (start / 8 >= data.len || end / 8 >= data.len)
    {
        return;
    }
    int64_t mask = ~(((uint64_t)-1) << (end-start + 1));
    value_new &= mask;

    int start_full = start / 8;
    int start_part = start % 8;
    
    unsigned char* bytes = data.data + start_full;
    
    // Extract up to 7 bytes of data
    int64_t value = 0;
    for (int i = 0; i < 7 && i < data.len; i++)
    {
        value += (int64_t)bytes[i] << (i*8);
    }
    
    value |= value_new << start_part;
    
    for (int i = 0; i < 7 && i < data.len; i++)
    {
        bytes[i] = (value >> (i*8)) & 0xFF;
    }
}

bool read_file(const char* path, binary_data* data, bool text)
{
    int offset = text ? 1 : 0;
    int fd = open(path, O_RDONLY);
    if(fd == -1) {
        error("Cant read file %s\n", path)
    }
    data->len = (int)lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    
    data->data = malloc(data->len + offset);
    read(fd, data->data, data->len);
    
    if (text)
    {
        data->data[data->len] = 0; /* Null termiante, for strings... */
        data->len++;
    }
    
    close(fd);
    return true;
}

bool write_file(const char* path, binary_data data)
{
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC);
    if(fd == -1) {
        return false;
    }
    
    write(fd, data.data, data.len);
    close(fd);
    
    return true;
}

bool binary_data_sub(binary_data original, binary_data* data_out, int start, int len)
{
    if (start + len > original.len)
    {
        data_out->data = 0;
        data_out->len = 0;
        return false;
    }
    data_out->data = original.data + start;
    data_out->len = len;
    return true;
}

int find_in_bytes(unsigned char* source, unsigned char* find, int source_len, int find_len)
{
    for (int i = 0; i < source_len - find_len; i++)
    {
        if (memcmp(source + i, find, find_len) == 0)
            return i;
    }
    
    return -1;
}

static void dump_byte(unsigned char c, char* line1, char* line2, char* line3, int pos)
{
    for(int i = 0; i < 8; i++)
    {
        unsigned char bit = (c >> i) & 1;
        line1[i*2] = bit ? '1' : '0';
        if (i == 0 || i == 4)
        {
            line2[i*2] ='|';
            int num = pos + i;
            char num_str[10];
            sprintf(num_str, "%d", num);
            line3[i*2] = num_str[0];
            if (num > 9)
            {
                line3[i*2 + 1] = num_str[1];
            }
        }
    }
}
void dump_to_binary(unsigned char* data, int len)
{
    char line1[255] = {0};
    char line2[255] = {0};
    char line3[255] = {0};
    memset(line1, ' ', 255);
    memset(line2, ' ', 255);
    memset(line3, ' ', 255);
    for (int i = 0; i < len; i++)
    {
        dump_byte(data[i], line1 + i*18, line2 + i*18, line3 + i*18, i*8);
    }
    line1[len*18] = 0;
    line2[len*18] = 0;
    line3[len*18] = 0;
    printf("%s\n%s\n%s\n", line1, line2, line3);
}

void dump_to_dec(unsigned char* data, int len)
{
    printf("Dump: ");
    for (int i = 0; i < len; i++)
    {
        printf("%d, ", data[i]);
    }
    printf("\n");
}

void dump_to_hex(unsigned char* data, int len)
{
    printf("Dump: ");
    for (int i = 0; i < len; i++)
    {
        printf("0x%02X, ", data[i]);
    }
    printf("\n");
}

int count_bits(int value)
{
    int ret = 0;
    while(value > 0)
    {
        ret += value & 1;
        value >>= 1;
    }
    return ret;
}

bool disassemble_bytecode_to_text(binary_data data_bytecode, binary_data* data_text, bool print_offsets)
{
    instruction* instructions;
    check(disassemble_bytecode_to_structs(data_bytecode, &instructions));
    check(disassemble_structs_to_text(instructions, data_text, print_offsets));
    
    destroy_instruction_list(instructions);
    
    return true;
}

bool assemble_text_to_bytecode(binary_data data_text, binary_data* data_bytecode)
{
    instruction* instructions;
    check(assemble_text_to_structs(data_text, &instructions));
    check(assemble_structs_to_bytecode(instructions, data_bytecode));
    
    destroy_instruction_list(instructions);
    
    return true;
}

void dump_disassembly(binary_data data_bytecode)
{
    instruction* instructions;
    disassemble_bytecode_to_structs(data_bytecode, &instructions);
    
    binary_data data_disassembly = {0};
    disassemble_structs_to_text(instructions, &data_disassembly, true);
    printf("%s\n", data_disassembly.data);
    
    destroy_instruction_list(instructions);
    free(data_disassembly.data);
}
