#ifndef helper_h
#define helper_h

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

#endif /* helper_h */
