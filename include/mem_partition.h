#ifndef MEM_PARTITION_H
#define MEM_PARTITION_H

#include "mem_block.h"
#include "common.h"

#define MAX_PT_NUM 8            // 最大分区数
#define MAX_PT_SIZE 0x2000000   // 分区最大内存32M

typedef struct MemPartitionStru {
    U8 *ptAddr;
    U32 ptSize;
    U32 ptIndex;
    MemBlock *freeMemList[MAX_BLOCK_TYPE_NUM];
    MemBlock *usedMemList[MAX_BLOCK_TYPE_NUM];

    // 申请内存
    U8 *(*MemAlloc)(struct MemPartitionStru *this, U32 size);
    // 释放内存
    U32 (*MemFree)(struct MemPartitionStru *this, void *mem);
} MemPartition;


// 内存分区数组
extern MemPartition *g_memPtCtrls[MAX_PT_NUM];

// 创建分区
MemPartition *CreateMemPt(U32 ptIndex, U32 ptSize);
// 删除分区
void DeleteMemPt(MemPartition *this);

#endif