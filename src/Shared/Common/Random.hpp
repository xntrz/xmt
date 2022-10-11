#pragma once


DLLSHARED void RndInitialize(void);
DLLSHARED void RndTerminate(void);
DLLSHARED int32 RndInt32(void);
DLLSHARED int32 RndInt32(int32 Begin, int32 End);
DLLSHARED uint32 RndUInt32(void);
DLLSHARED uint32 RndUInt32(uint32 Begin, uint32 End);
DLLSHARED float RndReal32(void);
DLLSHARED float RndReal32(float Begin, float End);