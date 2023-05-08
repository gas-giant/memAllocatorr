#include "mem_partition.h"
#include <stdlib.h>

// 内存分区数组
MemPartition *g_memPtCtrls[MAX_PT_NUM] = {NULL};

// 获取从左至右第一个非零bit所在的下标
static U32 GetFirstNoneZeroBitIndex(U32 n)
{
    for (U32 i = 31; i >= 0; --i) {
        if (((1 << i) & n) != 0) {
            return i;
        }
    }
    // 若n为0，则返回0
    return 0;
}

// 申请内存
U8 *MemAllocFsc(MemPartition *this, U32 size)
{
    if ((this == NULL) || (size == 0) || (size > (MAX_PT_SIZE - sizeof(MemBlock) - sizeof(U64)))) {
        return NULL;
    }
    // 申请的内存大小对应的类型[输入类型]
    U32 inputType = GetFirstNoneZeroBitIndex(size);

    // 1. 从比输入类型大一号的类型开始查找空闲内存块
    U32 curType = inputType + 1;
    MemBlock *curMemBlock = NULL;
    // 若当前类型的内存块不够划分出一份memblock头和尾，则要从能满足条件的类型开始查找
    if ((1 << (inputType + 1)) < (size + sizeof(MemBlock) + sizeof(U64))) {
        curType = GetFirstNoneZeroBitIndex(size + sizeof(MemBlock) + sizeof(U64));
    }
    // 开始查找非空链表
    for (; curType < MAX_BLOCK_TYPE_NUM; ++curType) {
        if (this->freeMemList[curType]->next != NULL) {
            curMemBlock = this->freeMemList[curType]->next;
            break;
        }
    }

    // 1.1 找不到非空链表，从输入类型开始查找空闲内存块
    if (curMemBlock == NULL) {
        curMemBlock = this->freeMemList[inputType]->next;
        // 查找满足大小的空闲内存块
        while (curMemBlock != NULL) {
            if (curMemBlock->size >= size) {
                break;
            }
            curMemBlock = curMemBlock->next;
        }
        // 找不到空闲内存块，申请失败
        if (curMemBlock == NULL) {
            return NULL;
        }
    }
    // 1.2 找到了非空链表，取其第一个内存块

    // 2. 当前内存块分裂
    MemBlock *buddyMemBlock = NULL;
    if (curMemBlock->size > size + sizeof(MemBlock) + sizeof(U64)) {
        // buddy块
        buddyMemBlock = (MemBlock *)((U8 *)(curMemBlock) + sizeof(MemBlock) + size + sizeof(U64));
        buddyMemBlock->size = curMemBlock->size - size - sizeof(U64) - sizeof(MemBlock);
        buddyMemBlock->type = GetFirstNoneZeroBitIndex(buddyMemBlock->size);
        *((U32 *)((U8 *)(buddyMemBlock) + sizeof(MemBlock) + buddyMemBlock->size)) = buddyMemBlock->size;
        *((U32 *)((U8 *)(buddyMemBlock) + sizeof(MemBlock) + buddyMemBlock->size + sizeof(U32))) = buddyMemBlock->type;
        // cur块
        curMemBlock->size = size;
        curMemBlock->type = inputType;
        *((U32 *)((U8 *)(buddyMemBlock) - sizeof(U64))) = size;
        *((U32 *)((U8 *)(buddyMemBlock) - sizeof(U32))) = inputType;
    }

    // 3. 链表删除原内存块
    curMemBlock->prev->next = curMemBlock->next;
    if (curMemBlock->next != NULL) {
        curMemBlock->next->prev = curMemBlock->prev;
    }
    
    // 4. 新内存块加入新链表
    // 当前内存块加入已用内存块链表
    curMemBlock->isUsed = 1;
    curMemBlock->next = this->usedMemList[curMemBlock->type]->next;
    curMemBlock->prev = this->usedMemList[curMemBlock->type];
    curMemBlock->prev->next = curMemBlock;
    if (curMemBlock->next != NULL) {
        curMemBlock->next->prev = curMemBlock;
    }
    // buddy内存块加入空闲内存块链表
    if (buddyMemBlock != NULL) {
        buddyMemBlock->isUsed = 0;
        buddyMemBlock->next = this->freeMemList[buddyMemBlock->type]->next;
        buddyMemBlock->prev = this->freeMemList[buddyMemBlock->type];
        buddyMemBlock->prev->next = buddyMemBlock;
        if (buddyMemBlock->next != NULL) {
            buddyMemBlock->next->prev = buddyMemBlock;
        }
    }

    // 5. 返回申请的内存地址
    return (U8 *)(curMemBlock) + sizeof(MemBlock);
}

// 释放内存
U32 MemFreeFsc(MemPartition *this, void *mem)
{
    if (this == NULL || mem == NULL) {
        return MEM_ALLOCATOR_PARA_ERROR;
    }
    // 当前内存块
    MemBlock *curMemBlock = (MemBlock *)((U8 *)mem - sizeof(MemBlock));
    // 后一个相邻内存块。若当前内存块是分区高地址边界，则不存在后一个相邻内存块
    MemBlock *backMemBlock = (MemBlock *)((U8 *)mem + curMemBlock->size + sizeof(U64));
    if ((U64)backMemBlock >= (U64)(this->ptAddr + this->ptSize)) {
        backMemBlock = NULL;
    }
    // 前一个相邻内存块。若当前内存块是分区低地址边界，则不存在前一个相邻内存块
    MemBlock *frontMemBlock = NULL;
    if ((U64)curMemBlock > (U64)this->ptAddr) {
        frontMemBlock = (MemBlock *)((U8 *)curMemBlock - *(U32 *)((U8 *)curMemBlock - sizeof(U64)) - sizeof(MemBlock));
    }

    // 1.删除已用内存块链表中的curMemBlock
    curMemBlock->isUsed = 0;
    curMemBlock->prev->next = curMemBlock->next;
    if (curMemBlock->next != NULL) {
        curMemBlock->next->prev = curMemBlock->prev;
    }

    // 2.合并后面相邻的空闲内存块
    if (backMemBlock != NULL && backMemBlock->isUsed == 0) {
        // 删除空闲链表中的backMemBlock
        backMemBlock->prev->next = backMemBlock->next;
        if (backMemBlock->next != NULL) {
            backMemBlock->next->prev = backMemBlock->prev;
        }

        // 合并
        curMemBlock->size += sizeof(U64) + sizeof(MemBlock) + backMemBlock->size;
        curMemBlock->type = GetFirstNoneZeroBitIndex(curMemBlock->size);
        *(U32 *)((U8 *)curMemBlock + sizeof(MemBlock) + curMemBlock->size) = curMemBlock->size;
        *(U32 *)((U8 *)curMemBlock + sizeof(MemBlock) + curMemBlock->size + sizeof(U32)) = curMemBlock->type;
        backMemBlock = NULL;

        // curMemBlock插入空闲链表
        curMemBlock->next = this->freeMemList[curMemBlock->type]->next;
        curMemBlock->prev = this->freeMemList[curMemBlock->type];
        curMemBlock->prev->next = curMemBlock;
        if (curMemBlock->next != NULL) {
            curMemBlock->next->prev = curMemBlock;
        }
    }

    // 3.合并前面相邻的空闲内存块
    if (frontMemBlock != NULL && frontMemBlock->isUsed == 0) {
        // 删除空闲链表中的frontMemBlock
        frontMemBlock->prev->next = frontMemBlock->next;
        if (frontMemBlock->next != NULL) {
            frontMemBlock->next->prev = frontMemBlock->prev;
        }
        
        // 合并
        frontMemBlock->size += sizeof(U64) + sizeof(MemBlock) + curMemBlock->size;
        frontMemBlock->type = GetFirstNoneZeroBitIndex(frontMemBlock->size);
        *(U32 *)((U8 *)frontMemBlock + sizeof(MemBlock) + frontMemBlock->size) = frontMemBlock->size;
        *(U32 *)((U8 *)frontMemBlock + sizeof(MemBlock) + frontMemBlock->size + sizeof(U32)) = frontMemBlock->type;
        curMemBlock = frontMemBlock;
        frontMemBlock = NULL;

        // curMemBlock插入空闲链表
        curMemBlock->next = this->freeMemList[curMemBlock->type]->next;
        curMemBlock->prev = this->freeMemList[curMemBlock->type];
        curMemBlock->prev->next = curMemBlock;
        if (curMemBlock->next != NULL) {
            curMemBlock->next->prev = curMemBlock;
        }
    }

    return MEM_ALLOCATOR_OK;
}

// 创建分区
MemPartition *CreateMemPt(U32 ptIndex, U32 ptSize)
{
    // 1. 申请内存，分区控制信息初始化
    if ((ptIndex >= MAX_PT_NUM) || (ptSize == 0) || (ptSize > MAX_PT_SIZE)) {
        return NULL;
    }
    MemPartition *ptCtrl = (MemPartition *)malloc(ptSize + sizeof(MemPartition));
    if (ptCtrl == NULL) {
        return NULL;
    }
    ptCtrl->ptAddr = (U8 *)ptCtrl + sizeof(MemPartition);
    ptCtrl->ptSize = ptSize;
    ptCtrl->ptIndex = ptIndex;
    g_memPtCtrls[ptIndex] = ptCtrl;

    // 2. 函数指针初始化
    ptCtrl->MemAlloc = MemAllocFsc;
    ptCtrl->MemFree = MemFreeFsc;

    // 3. 内存块链表初始化, 头节点初始化为非法值
    for (U32 i = 0; i < MAX_BLOCK_TYPE_NUM; ++i) {
        // 空闲内存块链表
        ptCtrl->freeMemList[i] = (MemBlock *)malloc(sizeof(MemBlock) + sizeof(U64));
        // 内存块头部非法值
        ptCtrl->freeMemList[i]->next = NULL;
        ptCtrl->freeMemList[i]->prev = NULL;
        ptCtrl->freeMemList[i]->size = 0;                                                   // 非法值
        ptCtrl->freeMemList[i]->type = MAX_BLOCK_TYPE_NUM;                                  // 非法值
        ptCtrl->freeMemList[i]->isUsed = 0;
        // 内存块尾部非法值(U32 size + U32 type)
        *((U64 *)((U8 *)ptCtrl->freeMemList[i] + sizeof(MemBlock))) = MAX_BLOCK_TYPE_NUM;   // 非法值

        // 已用内存块链表
        ptCtrl->usedMemList[i] = (MemBlock *)malloc(sizeof(MemBlock) + sizeof(U64));
        // 内存块头部非法值
        ptCtrl->usedMemList[i]->next = NULL;
        ptCtrl->usedMemList[i]->prev = NULL;
        ptCtrl->usedMemList[i]->size = 0;                                                   // 非法值
        ptCtrl->usedMemList[i]->type = MAX_BLOCK_TYPE_NUM;                                  // 非法值
        ptCtrl->usedMemList[i]->isUsed = 1;
        // 内存块尾部非法值(U32 size + U32 type)
        *((U64 *)((U8 *)ptCtrl->usedMemList[i] + sizeof(MemBlock))) = MAX_BLOCK_TYPE_NUM;   // 非法值
    }
    
    // 4. 当前空闲内存初始化为最大的空闲内存块，加入空闲链表
    MemBlock *maxBlock = (MemBlock *)ptCtrl->ptAddr;
    // 初始化内存块头部
    maxBlock->next = NULL;
    maxBlock->prev = NULL;
    maxBlock->size = ptSize - sizeof(MemBlock) - sizeof(U64);       // 掐头去尾
    maxBlock->type = GetFirstNoneZeroBitIndex(maxBlock->size);      // 从左至右数第一个非零比特的下标
    maxBlock->isUsed = 0;
    // 初始化内存块尾部
    U8 *maxBlockTail = (U8 *)ptCtrl->ptAddr + ptSize - sizeof(U64); // 最大内存块尾部信息
    *((U32 *)(maxBlockTail++)) = maxBlock->size;
    *((U32 *)maxBlockTail) = maxBlock->type;
    // 最大空闲内存块加入空闲内存块链表
    ptCtrl->freeMemList[maxBlock->type]->next = maxBlock;
    maxBlock->prev = ptCtrl->freeMemList[maxBlock->type];

    return ptCtrl;
}

// 删除分区
void DeleteMemPt(MemPartition *this)
{
    // 分区数组删除该分区
    g_memPtCtrls[this->ptIndex] = NULL;
    // 分区内的内存块链表释放内存
    for (U32 i = 0; i < MAX_BLOCK_TYPE_NUM - 1; ++i) {
        free(this->freeMemList[i]);
        free(this->usedMemList[i]);
    }
    // 释放该分区的内存
    free(this);
    this = NULL;
}