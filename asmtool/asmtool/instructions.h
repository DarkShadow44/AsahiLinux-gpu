#ifndef instructions_h
#define instructions_h

enum opcodes
{
    OPCODE_STOP = 0x88,
    OPCODE_RET = 0x14,
    OPCODE_STORE = 0x45,
    OPCODE_MOV = 0x62,
};

enum format_load_store
{
    FORMAT_I8       = 0,
    FORMAT_I16      = 1,
    FORMAT_I32      = 2,
    FORMAT_UNK1     = 3,
    FORMAT_U8NORM   = 4,
    FORMAT_S8NORM   = 5,
    FORMAT_U16NORM  = 6,
    FORMAT_S16NORM  = 7,
    FORMAT_RGB10A2  = 8,
    FORMAT_UNK2     = 9,
    FORMAT_SRGBA8   = 10,
    FORMAT_UNK3     = 11,
    FORMAT_RG11B10F = 12,
    FORMAT_RGB9E5   = 13,
    FORMAT_UNK4     = 14,
    FORMAT_UNK5     = 15,
};

struct format_load_store_list
{
    enum format_load_store format;
    const char* name;
};

static char* format_names [] =
{
    "i8",
    "i16",
    "i32",
    "UNKNOWN",
    "u8norm",
    "s8norm",
    "u16norm",
    "s16norm",
    "rgb10a2",
    "UNKNOWN",
    "srgb8",
    "UNKNOWN",
    "rg11b10f",
    "rgb9e5",
    "UNKNOWN",
    "UNKNOWN",
};

#endif /* instructions_h */
