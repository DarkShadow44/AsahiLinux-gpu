/*
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

/* Original source: https://github.com/dougallj/applegpu/blob/main/hwtestbed/replacer.c (BSD 3 Clause) */

#include <mach/mach.h>
#include <IOKit/IOKitLib.h>
#include "../asmtool/hook.h"

static hook_functions functions;

void set_hook_functions(hook_functions new_functions, hook_functions* originals)
{
    functions = new_functions;
    originals->fIOConnectCallMethod = IOConnectCallMethod;
}

#define DYLD_INTERPOSE(_replacment,_replacee) \
    __attribute__((used)) static struct{ const void* replacment; const void* replacee; } _interpose_##_replacee \
    __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacment, (const void*)(unsigned long)&_replacee };

kern_return_t wrap_IOConnectCallMethod(mach_port_t connection, uint32_t selector, const uint64_t* input, uint32_t inputCnt, const void* inputStruct, size_t inputStructCnt, uint64_t* output, uint32_t* outputCnt, void* outputStruct, size_t* outputStructCntP)
{
    return functions.fIOConnectCallMethod(connection, selector, input, inputCnt, inputStruct, inputStructCnt, output, outputCnt, outputStruct, outputStructCntP);
}

kern_return_t wrap_IOConnectCallStructMethod(mach_port_t connection, uint32_t selector, const void* inputStruct, size_t inputStructCnt, void* outputStruct, size_t* outputStructCntP)
{
    return wrap_IOConnectCallMethod(connection, selector, NULL, 0, inputStruct, inputStructCnt, NULL, NULL, outputStruct, outputStructCntP);
}


kern_return_t wrap_IOConnectCallScalarMethod(mach_port_t connection, uint32_t selector, const uint64_t* input, uint32_t inputCnt, uint64_t* output, uint32_t* outputCnt)
{
    return wrap_IOConnectCallMethod(connection, selector, input, inputCnt, NULL, 0, output, outputCnt, NULL, NULL);
}

DYLD_INTERPOSE(wrap_IOConnectCallMethod, IOConnectCallMethod);
DYLD_INTERPOSE(wrap_IOConnectCallStructMethod, IOConnectCallStructMethod);
DYLD_INTERPOSE(wrap_IOConnectCallScalarMethod, IOConnectCallScalarMethod);
