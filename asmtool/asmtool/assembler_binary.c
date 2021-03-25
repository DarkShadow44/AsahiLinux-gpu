#include "gpu.h"
#include "instructions.h"

void SET_BITS(binary_data data, int start, int end, uint64_t value_new)
{
    assert(start / 8 < data.len && end / 8 < data.len);
    uint64_t max = 1 << (end - start + 1);
    assert(value_new < max);

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

