/*
 Original sources:
 https://github.com/Thog/gpu/blob/metallib/metal_compiler.m
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
 
 https://github.com/dougallj/applegpu/blob/main/hwtestbed/replacer.c (BSD 3 Clause)
  * Copyright (c) 2021 Dougall Johnson
  * Copyright (c) 2020 Asahi Linux contributors
  * Copyright (c) 1998-2014 Apple Computer, Inc. All rights reserved.
  * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
  *
  * IOKit prototypes and stub implementations from upstream IOKitLib sources.
  * DYLD_INTERPOSE macro from dyld source code.  All other code in the file is
  * by Asahi Linux contributors.
  *
  * @APPLE_LICENSE_HEADER_START@
  *
  * This file contains Original Code and/or Modifications of Original Code
  * as defined in and that are subject to the Apple Public Source License
  * Version 2.0 (the 'License'). You may not use this file except in
  * compliance with the License. Please obtain a copy of the License at
  * http://www.opensource.apple.com/apsl/ and read it before using this
  * file.
  *
  * The Original Code and all software distributed under the License are
  * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
  * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
  * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
  * Please see the License for the specific language governing rights and
  * limitations under the License.
  *
  * @APPLE_LICENSE_HEADER_END@
 */


#include "gpu.h"
#import <MetalKit/MetalKit.h>
#include <libkern/OSCacheControl.h>
#include "hook.h"

static hook_functions originals;

static binary_data code_to_insert;
static bool flag_enable_code_hook = false;
static bool flag_hardware_init = false;

enum agx_alloc_type {
    AGX_ALLOC_REGULAR = 0,
    AGX_ALLOC_MEMMAP = 1,
    AGX_ALLOC_CMDBUF = 2,
    AGX_NUM_ALLOC,
};

struct agx_allocation {
    enum agx_alloc_type type;
    size_t size;

    /* Index unique only up to type */
    unsigned index;

    /* If CPU mapped, CPU address. NULL if not mapped */
    void *map;

    /* If type REGULAR, mapped GPU address */
    uint64_t gpu_va;
};

enum agx_selector {
    AGX_SELECTOR_SET_API = 0x7,
    AGX_SELECTOR_ALLOCATE_MEM = 0xA,
    AGX_SELECTOR_CREATE_CMDBUF = 0xF,
    AGX_SELECTOR_SUBMIT_COMMAND_BUFFERS = 0x1E,
    AGX_SELECTOR_GET_VERSION = 0x23,
    AGX_NUM_SELECTORS = 0x30
};


unsigned MAP_COUNT = 0;
#define MAX_MAPPINGS 4096
struct agx_allocation mappings[MAX_MAPPINGS];

mach_port_t metal_connection = 0;

kern_return_t wrap_IOConnectCallMethod(mach_port_t connection, uint32_t selector, const uint64_t* input, uint32_t inputCnt, const void* inputStruct, size_t inputStructCnt, uint64_t* output, uint32_t* outputCnt, void* outputStruct, size_t* outputStructCntP)
{
    /* Heuristic guess which connection is Metal, skip over I/O from everything else */
    bool bail = false;

   /* if (metal_connection == 0) {
        if (selector == AGX_SELECTOR_SET_API)
            metal_connection = connection;
        else
            bail = true;
    } else if (metal_connection != connection)
        bail = true;*/

    if (bail || (!flag_enable_code_hook && !flag_hardware_init))
        return originals.fIOConnectCallMethod(connection, selector, input, inputCnt, inputStruct, inputStructCnt, output, outputCnt, outputStruct, outputStructCntP);

    /* Check the arguments make sense */
    assert((input != NULL) == (inputCnt != 0));
    assert((inputStruct != NULL) == (inputStructCnt != 0));
    assert((output != NULL) == (outputCnt != 0));
    assert((outputStruct != NULL) == (outputStructCntP != 0));

    /* Dump inputs */
    switch (selector) {
    case AGX_SELECTOR_SET_API:
        assert(input == NULL && output == NULL && outputStruct == NULL);
        assert(inputStruct != NULL && inputStructCnt == 16);
        assert(((uint8_t *) inputStruct)[15] == 0x0);
        break;

    case AGX_SELECTOR_SUBMIT_COMMAND_BUFFERS:
        assert(output == NULL && outputStruct == NULL);
        assert(inputStructCnt == 40);
        assert(inputCnt == 1);

        bool fs = false;

        for (unsigned i = 0; i < MAP_COUNT; ++i) {
            unsigned offset = fs ? 0x1380 : 0x13c0;
            
            if (!mappings[i].gpu_va && !(mappings[i].type))
            {
                /*dump_to_dec(mappings[i].map + offset, 200);
                binary_data dump;
                dump.data = mappings[i].map + offset;
                dump.len = 0x470;
                write_file("/Users/fabian/Programming/GPU/applegpu/mapping.dump", dump);*/
            }

            if (!mappings[i].gpu_va && !(mappings[i].type)) {
                if (!flag_hardware_init)
                {
                    unsigned char buff[] = {
                        0x0e, 0x45, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, // iadd r17, u0, 0
                        0x0e, 0x49, 0x82, 0x01, 0x00, 0x00, 0x00, 0x00, // iadd r18, u1, 0
                        0x0e, 0x4d, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, // iadd r19, u0, 0
                        0x0e, 0x51, 0x82, 0x01, 0x00, 0x00, 0x00, 0x00, // iadd r20, u1, 0
                    };
                    int buff_size = sizeof(buff);
                    unsigned char* pos_buff = (unsigned char*)mappings[i].map + offset;
                    unsigned char* pos_data = pos_buff + buff_size;
                    memcpy(pos_buff, buff, buff_size);
                    memcpy(pos_data, code_to_insert.data, code_to_insert.len);
                    pos_data[code_to_insert.len - 2] = 0x88; // STOP instead of ret
                    pos_data[code_to_insert.len - 1] = 0x00; // STOP instead of ret
                    sys_dcache_flush(pos_buff, buff_size + code_to_insert.len);

                }
            }
        }
    }

    /* Invoke the real method */
    kern_return_t ret = originals.fIOConnectCallMethod(connection, selector, input, inputCnt, inputStruct, inputStructCnt, output, outputCnt, outputStruct, outputStructCntP);


    /* Track allocations for later analysis (dumping, disassembly, etc) */
    switch (selector) {
    case AGX_SELECTOR_CREATE_CMDBUF: {
         assert(inputCnt == 2);
        assert((*outputStructCntP) == 0x10);
        uint64_t *inp = (uint64_t *) input;
        assert(inp[1] == 1 || inp[1] == 0);
        uint64_t *ptr = (uint64_t *) outputStruct;
        uint32_t *words = (uint32_t *) (ptr + 1);
        unsigned mapping = MAP_COUNT++;
        assert(mapping < MAX_MAPPINGS);
        mappings[mapping] = (struct agx_allocation) {
            .index = words[1],
            .map = (void *) *ptr,
            .size = words[0],
            .type = inp[1] ? AGX_ALLOC_CMDBUF : AGX_ALLOC_MEMMAP
        };
        break;
    }
    
    case AGX_SELECTOR_ALLOCATE_MEM: {
        assert((*outputStructCntP) == 0x50);
        uint64_t *iptrs = (uint64_t *) inputStruct;
        uint64_t *ptrs = (uint64_t *) outputStruct;
        uint64_t gpu_va = ptrs[0];
        uint64_t cpu = ptrs[1];
        uint64_t cpu_fixed_1 = iptrs[6];
        //uint64_t cpu_fixed_2 = iptrs[7]; /* xxx what's the diff? */
        if (cpu && cpu_fixed_1)
            assert(cpu == cpu_fixed_1);
        else if (cpu == 0)
            cpu = cpu_fixed_1;
        uint64_t size = ptrs[4];
        unsigned mapping = MAP_COUNT++;
        //printf("allocate gpu va %llx, cpu %llx, 0x%llx bytes (%u)\n", gpu_va, cpu, size, mapping);
        assert(mapping < MAX_MAPPINGS);
        mappings[mapping] = (struct agx_allocation) {
            .type = AGX_ALLOC_REGULAR,
            .size = size,
            .index = iptrs[3] >> 32ull,
            .gpu_va = gpu_va,
            .map = (void *) cpu,
        };
    }

    default:
        break;
    }

    return ret;
}

void hardware_init(void)
{
    hook_functions functions = {0};
    functions.fIOConnectCallMethod = wrap_IOConnectCallMethod;
    set_hook_functions(functions, &originals);
    
    flag_hardware_init = true;
    binary_data dummy = {0};
    get_results_from_gpu(dummy, 0);
    flag_hardware_init = false;
}

void enable_code_hook(binary_data new_code_to_insert)
{
    code_to_insert = new_code_to_insert;
    flag_enable_code_hook = true;
    metal_connection = 0;
}

void disable_code_hook(void)
{
    flag_enable_code_hook = false;
}

bool compile_shader_to_metallib(char* sourcecode, binary_data* data)
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    const char* temp_path = "/tmp/temp.metallib";
    const char* temp_path2 = "file:///tmp/temp.metallib";

    NSString* destinationPath = [NSString stringWithUTF8String:temp_path2];
    NSString* programString = [NSString stringWithUTF8String:sourcecode];
    
    NSError* error;

    MTLCompileOptions* options = [MTLCompileOptions new];
    options.libraryType = MTLLibraryTypeDynamic;
    options.installName = [NSString stringWithFormat:@"@executable_path/userCreatedDylib.metallib"];

    id<MTLLibrary> lib = [device newLibraryWithSource:programString options:options error:&error];

    if(!lib && error)
    {
        NSString* error_desc = [error localizedDescription];
        const char* error_desc_c = [error_desc UTF8String];
        printf("Error: %s\n", error_desc_c);
        return false;
    }
    
    id<MTLDynamicLibrary> dynamicLib = [device newDynamicLibrary:lib error:&error];
    if(!dynamicLib && error)
    {
        return false;
    }

    NSURL *destinationURL = [NSURL URLWithString:destinationPath];

    bool result = [dynamicLib serializeToURL:destinationURL error:&error];

    if (!result && error)
    {
        NSLog(@"Error serializing dynamic library: %@", error);
        return false;
    }
    
    read_file(temp_path, data, false);
    
    return true;
}

bool get_results_from_gpu(binary_data code, test_io* io)
{
    enable_code_hook(code);

    NSError *error = nil;

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device)
    {
        disable_code_hook();
        return false;
    }

    error = nil;
    //id<MTLLibrary> library = [device newDefaultLibrary];
    id<MTLLibrary> library = [device newLibraryWithFile:@"/Users/fabian/Programming/GPU/applegpu/hwtestbed/compute.metallib" error:&error];
    if (!library)
    {
        disable_code_hook();
        return false;
    }

    id<MTLFunction> kernelFunction = [library newFunctionWithName:@"add_arrays"];
    if (!kernelFunction)
    {
        disable_code_hook();
        return false;
    }
    
    error = nil;
    id<MTLComputePipelineState> computePipelineState = [device newComputePipelineStateWithFunction:kernelFunction error:&error];
    if (!computePipelineState)
        return false;

    id<MTLCommandQueue> commandQueue = [device newCommandQueue];
    if (!commandQueue)
        return false;

    int BUFFER_COUNT = 8;
    int count = TEST_BUFFER_SIZE;
    id<MTLBuffer> outputBuffers[BUFFER_COUNT];
    for (int i = 0; i < BUFFER_COUNT; i++) {
        outputBuffers[i] = [device newBufferWithLength:TEST_BUFFER_SIZE options:MTLResourceStorageModeShared];
        memset(outputBuffers[i].contents, 0, TEST_BUFFER_SIZE * sizeof(uint32_t));
        if (i == 0 && io)
        {
            for (int simd = 0; simd < count; simd++) {
                for (int r = 0; r < 4; r++) {
                    uint32_t *inputs = (uint32_t*)outputBuffers[r/4].contents;
                    inputs[(r & 3) + (simd * 4)] = io->buffer0[simd][r];
                }
            }
        }
    }

    id<MTLCommandBuffer> commandBuffer = commandQueue.commandBuffer;
    id<MTLComputeCommandEncoder> encoder = commandBuffer.computeCommandEncoder;
    
    [encoder setComputePipelineState:computePipelineState];

    for (int i = 0; i < BUFFER_COUNT; i++) {
        [encoder setBuffer:outputBuffers[i] offset:0 atIndex:i];
    }

    [encoder setThreadgroupMemoryLength:0x100 atIndex:0];

    MTLSize threadgroupsPerGrid = MTLSizeMake(count, 1, 1);

    NSUInteger threads = computePipelineState.maxTotalThreadsPerThreadgroup;
    if (count < threads)
        threads = count;

    MTLSize threadsPerThreadgroup = MTLSizeMake(threads, 1, 1);
    
    [encoder dispatchThreads:threadgroupsPerGrid threadsPerThreadgroup:threadsPerThreadgroup];
    [encoder endEncoding];

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    
    disable_code_hook();

    if (!flag_hardware_init)
    {
        if (io)
        {
            for (int simd = 0; simd < count; simd++) {
                for (int r = 0; r < 4; r++) {
                    uint32_t *outputs = (uint32_t*)outputBuffers[r/4].contents;
                    io->buffer0[simd][r] = outputs[(r & 3) + (simd * 4)];
                }
            }
        }
        else
        {
            printf("[\n");
            for (int r = 0; r < 4; r++) {
                printf("[");
                assert(r/4 < TEST_BUFFER_COUNT);
                for (int simd = 0; simd < count; simd++) {
                    uint32_t *outputs = (uint32_t*)outputBuffers[r/4].contents;
                    printf("0x%x,", outputs[(r & 3) + (simd * 4)]);
                }
                printf("],\n");
            }
            printf("]\n");
        }
    }

    return true;
}
