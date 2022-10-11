#pragma once


DLLSHARED void TimeInitialize(void);
DLLSHARED void TimeTerminate(void);
DLLSHARED uint32 TimeCurrentTick(void);
DLLSHARED uint32 TimeCurrentTickPrecise(void);
DLLSHARED uint32 TimeCurrentUnix32(void);
DLLSHARED uint32 TimeCurrentUnix32Ex(uint32* Ms);
DLLSHARED uint64 TimeCurrentUnix64(void);
DLLSHARED uint64 TimeCurrentUnix64Ex(uint32* Ms);
DLLSHARED void TimeCurrentLocal(uint32* Hour, uint32* Minute, uint32* Second, uint32* Ms);
DLLSHARED void TimeCurrentLocalEx(uint32* Hour, uint32* Minute, uint32* Second, uint32* Ms, uint32* Year, uint32* Month, uint32* Day);
DLLSHARED void TimeStampSlice(uint32 Timestamp, uint32* Hour, uint32* Minute, uint32* Second, uint32* Ms);