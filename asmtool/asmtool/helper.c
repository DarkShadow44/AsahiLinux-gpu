#include "helper.h"

#include <unistd.h>
#include <sys/fcntl.h>

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
