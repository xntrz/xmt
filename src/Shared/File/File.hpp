#pragma once

#include "FileTypes.hpp"


DLLSHARED bool FileInitialize(void);
DLLSHARED void FileTerminate(void);
DLLSHARED HOBJ FileOpen(const char* Path, const char* Access, void* Param = nullptr);
DLLSHARED void FileClose(HOBJ hFile);
DLLSHARED uint32 FileRead(HOBJ hFile, char* Buffer, uint32 BufferSize);
DLLSHARED uint32 FileWrite(HOBJ hFile, const char* Buffer, uint32 BufferSize);
DLLSHARED uint64 FileTell(HOBJ hFile);
DLLSHARED void FileSeek(HOBJ hFile, int64 Offset, FileSeek_t Seek);
DLLSHARED void FileSync(HOBJ hFile);
DLLSHARED bool FileSync(HOBJ hFile, uint32 Timeout);
DLLSHARED void FileFlush(HOBJ hFile);
DLLSHARED bool FileIsEof(HOBJ hFile);
DLLSHARED uint64 FileSize(HOBJ hFile);
DLLSHARED FileStat_t FileStat(HOBJ hFile);
DLLSHARED int32 FileError(HOBJ hFile);
DLLSHARED bool FileAbort(HOBJ hFile);
DLLSHARED bool FileExist(const char* Path);
DLLSHARED bool FileDelete(const char* Path);