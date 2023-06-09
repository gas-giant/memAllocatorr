#include "mem_partition.h"
#include <stdio.h>

// 输出分区所有字节
static void PrintMemPtBytes(MemPartition *pt)
{
    if (pt == NULL) {
        printf("pt is NULL\n");
        return;
    }
    for (int i = 0; i < pt->ptSize; ++i) {
        if (i % 16 == 0) {
            printf("\n0x%016llx:", pt->ptAddr + i);
            // printf("\n%#x:", pt->ptAddr + i);
        }
        if (i % 4 == 0) {
            printf(" ");
        }
        printf(" %02x", *(pt->ptAddr + i));
    }
}

static void UT_MEMALLOCATOR_01(void)
{
    // memBlock head = 4 * 4 + 8 * 2 = 32
    // memBlock tail = 8
    MemPartition *pt = CreateMemPt(0, 136);

    // need: 4 + 4 + 8 + 40 * 3 = 16 + 120 = 136
    U32 *a1 = (U32 *)pt->MemAlloc(pt, sizeof(U32));
    U32 *a2 = (U32 *)pt->MemAlloc(pt, sizeof(U32));
    U64 *a3 = (U64 *)pt->MemAlloc(pt, sizeof(U64));
    printf("a1 = %#x\na2 = %#x\na3 = %#x\n", a1, a2, a3);

    *a1 = 0x11223344;
    *a2 = 0xaabbccdd;
    *a3 = 0x5555666677778888;

    PrintMemPtBytes(pt);

    DeleteMemPt(pt);
}

int main(void)
{
    UT_MEMALLOCATOR_01();
    return 0;
}