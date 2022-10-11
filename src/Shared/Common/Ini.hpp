#pragma once

typedef bool(*IniKeyEnumCallback_t)(const char* Segment, const char* Key, const char* Value, void* Param);


DLLSHARED HOBJ IniNew(void);
DLLSHARED HOBJ IniOpen(const char* Buffer, int32 BufferSize);
DLLSHARED bool IniSave(HOBJ hIni, char* Buffer, int32* BufferSize);
DLLSHARED void IniClose(HOBJ hIni);
DLLSHARED bool IniSegmentNew(HOBJ hIni, const char* Segment);
DLLSHARED bool IniSegmentSearch(HOBJ hIni, const char* Segment);
DLLSHARED bool IniSegmentDelete(HOBJ hIni, const char* Segment);
DLLSHARED bool IniKeyNew(HOBJ hIni, const char* Segment, const char* Key);
DLLSHARED bool IniKeySearch(HOBJ hIni, const char* Segment, const char* Key);
DLLSHARED bool IniKeyDelete(HOBJ hIni, const char* Segment, const char* Key);
DLLSHARED void IniKeyEnum(HOBJ hIni, IniKeyEnumCallback_t pfnEnumCB, void* Param = nullptr);
DLLSHARED bool IniKeyWriteInt(HOBJ hIni, const char* Segment, const char* Key, int32 Value);
DLLSHARED bool IniKeyWriteUInt(HOBJ hIni, const char* Segment, const char* Key, uint32 Value);
DLLSHARED bool IniKeyWriteHex(HOBJ hIni, const char* Segment, const char* Key, uint32 Value);
DLLSHARED bool IniKeyWriteFloat(HOBJ hIni, const char* Segment, const char* Key, float Value);
DLLSHARED bool IniKeyWriteDouble(HOBJ hIni, const char* Segment, const char* Key, double Value);
DLLSHARED bool IniKeyWriteBool(HOBJ hIni, const char* Segment, const char* Key, bool Value);
DLLSHARED bool IniKeyWriteString(HOBJ hIni, const char* Segment, const char* Key, const char* Buffer, int32 BufferSize);
DLLSHARED bool IniKeyWriteString(HOBJ hIni, const char* Segment, const char* Key, const char* Value);
DLLSHARED bool IniKeyReadInt(HOBJ hIni, const char* Segment, const char* Key, int32* Value);
DLLSHARED bool IniKeyReadUInt(HOBJ hIni, const char* Segment, const char* Key, uint32* Value);
DLLSHARED bool IniKeyReadHex(HOBJ hIni, const char* Segment, const char* Key, uint32* Value);
DLLSHARED bool IniKeyReadFloat(HOBJ hIni, const char* Segment, const char* Key, float* Value);
DLLSHARED bool IniKeyReadDouble(HOBJ hIni, const char* Segment, const char* Key, double* Value);
DLLSHARED bool IniKeyReadBool(HOBJ hIni, const char* Segment, const char* Key, bool* Value);
DLLSHARED bool IniKeyReadString(HOBJ hIni, const char* Segment, const char* Key, char* Buffer, int32 BufferSize);
DLLSHARED bool IniKeyReadString(HOBJ hIni, const char* Segment, const char* Key, const char** Value);