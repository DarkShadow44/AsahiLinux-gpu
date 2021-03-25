/*
 * Copyright (C) 2021 Mary <me@thog.eu>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Original source: https://github.com/Thog/gpu/blob/metallib/disasm/macho_disasm.c */

#include "gpu.h"

// CPU types
#define CPU_TYPE_APPLE_GPU  (0x13 | CPU_ARCH_ABI64)
#define CPU_TYPE_AMD_GPU    (0x14 | CPU_ARCH_ABI64)
#define CPU_TYPE_INTEL_GPU  (0x15 | CPU_ARCH_ABI64)
#define CPU_TYPE_NVIDIA_GPU (0x16 | CPU_ARCH_ABI64)
#define CPU_TYPE_AIR        (0x17 | CPU_ARCH_ABI64)

#define CPU_SUBTYPE_AGX(f, m) ((cpu_subtype_t) (f) + ((m) << 4))

// AGX0
#define CPU_SUBTYPE_APPLE_S3    CPU_SUBTYPE_AGX(1, 2)

// AGX1
#define CPU_SUBTYPE_APPLE_GPU_A8   CPU_SUBTYPE_AGX(1, 1)
#define CPU_SUBTYPE_APPLE_GPU_A8X  CPU_SUBTYPE_AGX(1, 3)
#define CPU_SUBTYPE_APPLE_GPU_A9   CPU_SUBTYPE_AGX(1, 4)
#define CPU_SUBTYPE_APPLE_GPU_A10  CPU_SUBTYPE_AGX(1, 5)
#define CPU_SUBTYPE_APPLE_GPU_A10X CPU_SUBTYPE_AGX(1, 6)

// AGX2
#define CPU_SUBTYPE_APPLE_GPU_A11  CPU_SUBTYPE_AGX(2, 2)
#define CPU_SUBTYPE_APPLE_GPU_S4   CPU_SUBTYPE_AGX(2, 5)
#define CPU_SUBTYPE_APPLE_GPU_A12  CPU_SUBTYPE_AGX(2, 7)
#define CPU_SUBTYPE_APPLE_GPU_A12X CPU_SUBTYPE_AGX(2, 8)
#define CPU_SUBTYPE_APPLE_GPU_A13  CPU_SUBTYPE_AGX(2, 13)
#define CPU_SUBTYPE_APPLE_GPU_A14  CPU_SUBTYPE_AGX(2, 18)
#define CPU_SUBTYPE_APPLE_GPU_A14X CPU_SUBTYPE_AGX(2, 20)
#define CPU_SUBTYPE_APPLE_GPU_A12Z CPU_SUBTYPE_AGX(2, 100)

#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <mach-o/loader.h>
#include <mach-o/fat.h>

uint32_t swap_uint32(uint32_t value)
{
    value = ((value << 8) & 0xFF00FF00) | ((value >> 8) & 0xFF00FF);
    return (value << 16) | (value >> 16);
}

uint64_t swap_uint64(uint64_t value)
{
    value = ((value << 8) & 0xFF00FF00FF00FF00ULL) | ((value >> 8) & 0x00FF00FF00FF00FFULL);
    value = ((value << 16) & 0xFFFF0000FFFF0000ULL) | ((value >> 16) & 0x0000FFFF0000FFFFULL);
    return (value << 32) | (value >> 32);
}

typedef struct
{
    void *address;
    size_t size;
} section_info_t;

static int get_mach_text_section(section_info_t *text_section_info, char *segment_name, void *address, size_t size)
{
    struct mach_header_64* header = (struct mach_header_64*)address;

    if(header->magic == MH_MAGIC_64)
    {
        struct load_command *commands = (struct load_command*)&header[1];

        struct load_command *command = commands;

        for(int command_index = 0; command_index < header->ncmds; command_index++)
        {
            if (command->cmdsize == 0 || (uintptr_t)command + command->cmdsize - (uintptr_t)header > size)
            {
                return 2;
            }

            if (command->cmd == LC_SEGMENT_64)
            {
                struct segment_command_64* segment = (struct segment_command_64*)command;

                if(strncmp(segment->segname, segment_name, 16) == 0)
                {
                    struct section_64* sections = (struct section_64*)&segment[1];
                    for(int j = 0; j < segment->nsects; j++)
                    {
                        if(strncmp(sections[j].sectname, "__text", 16) == 0)
                        {
                            text_section_info->address = address + sections[j].offset;
                            text_section_info->size = sections[j].size;
                            return 0;
                        }
                    }
                }
            }

            command = (struct load_command*)((char*)command + command->cmdsize);
        }
        return 3;
    }
    else
    {
        return 1;
    }
}

static int get_agx_code_info(section_info_t *agx_text_section_info, void *file, size_t file_size)
{
    section_info_t text_section;
    bzero(&text_section, sizeof(section_info_t));

    uint32_t magic = *((uint32_t*)file);

    void *start = NULL;
    size_t size = 0;

    if (magic == FAT_CIGAM_64)
    {
        struct fat_header *header = (struct fat_header*)file;
        struct fat_arch_64 *archs = (struct fat_arch_64*)&header[1];

        for (int arch_index = 0; arch_index < swap_uint32(header->nfat_arch); arch_index++)
        {
            // Match on M1 GPU.
            if (swap_uint32(archs[arch_index].cputype) == CPU_TYPE_APPLE_GPU && swap_uint32(archs[arch_index].cpusubtype) == CPU_SUBTYPE_APPLE_GPU_A14X)
            {
                start = file + swap_uint64(archs[arch_index].offset);
                size = swap_uint64(archs[arch_index].size);

                break;
            }
        }

        if (start == NULL)
        {
            return 1;
        }
    }
    else if (magic == MH_MAGIC_64)
    {
        start = file;
        size = file_size;
    }
    else
    {
        return 1;
    }

    int ret = get_mach_text_section(&text_section, "__TEXT", start, size);

    if (ret == 0)
    {
        return get_mach_text_section(agx_text_section_info, "", text_section.address, text_section.size);
    }

    return ret;
}

bool get_bytecode_from_metallib(binary_data data, binary_data* data_out)
{
    section_info_t info;
    bzero(&info, sizeof(section_info_t));

    if(get_agx_code_info(&info, data.data, data.len) != 0)
    {
        return false;
    }
    
    data_out->data = malloc(info.size);
    data_out->len = (int)info.size;
    memcpy(data_out->data, info.address, info.size);

    return true;
}
