#pragma once

void AppMemInitialize(uint32 ReserveMemSize = 0);
void AppMemTerminate(void);
void AppMemTerminate2(void);
void AppMemGrabDiag(
    uint64* AllocNum,
    uint64* CrossThreadAlloc,
    uint64* SelfThreadAlloc,
    uint32* AllocatedBytes,
    uint32* LargestAllocSize,
    uint32* LargestAllocatedSize
);