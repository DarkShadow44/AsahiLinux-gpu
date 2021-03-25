#ifndef hook_h
#define hook_h

typedef kern_return_t (*pIOConnectCallMethod)( mach_port_t connection, uint32_t selector, const uint64_t* input, uint32_t inputCnt, const void* inputStruct, size_t inputStructCnt, uint64_t* output, uint32_t* outputCnt, void* outputStruct, size_t* outputStructCntP);

typedef struct _hook_functions
{
    pIOConnectCallMethod fIOConnectCallMethod;
} hook_functions;

void set_hook_functions(hook_functions new_functions, hook_functions* originals);

#endif /* hook_h */
