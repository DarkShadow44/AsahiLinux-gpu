#ifndef helper_h
#define helper_h

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 

typedef struct _binary_data
{
    unsigned char* data;
    unsigned int len;
} binary_data;

bool binary_data_sub(binary_data original, binary_data* data_out, int start, int len);

bool read_file(const char* path, binary_data* data);
bool write_file(const char* path, binary_data data);
int find_in_bytes(unsigned char* source, unsigned char* find, int source_len, int find_len);
void dump_to_binary(unsigned char* data, int len);
void dump_to_dec(unsigned char* data, int len);
void dump_to_hex(unsigned char* data, int len);

#endif /* helper_h */
