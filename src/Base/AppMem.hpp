#pragma once

void AppMemInitialize(uint32 ReserveMemSize = 0);
void AppMemTerminate(void);
void AppMemGrabDiag(
    int64* AllocNum,
    int64* CrossThreadAlloc,
    int64* SelfThreadAlloc,
    uint32* AllocatedBytes,
    uint32* LargestAllocSize,
    uint32* LargestAllocatedSize
);