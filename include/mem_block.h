#ifndef MEM_BLOCK_H
#define MEM_BLOCK_H

#include "common.h"

/* 
内存块类型     [0 -- 31]
对应的基准值为 [2^0 -- 2^31]
exp. [2^4]对应的内存块类型，其大小的取值范围x为 2^4 <= x <= 2^5-1
*/
#define MAX_BLOCK_TYPE_NUM 32     // 内存块种类数

typedef struct MemBlockStru {
    U32 size;
    U32 type;
    U32 isUsed;
    U32 pad;
    struct MemBlockStru *next;
    struct MemBlockStru *prev;
} MemBlock;

#endif