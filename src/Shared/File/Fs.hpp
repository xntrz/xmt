#pragma once

#include "FileTypes.hpp"


struct FileSystem_t
{
    char* Name;
    char* Path;
    FileSystem_t* Next;
    HOBJ(*Open)(FileSystem_t* Fs, const char* Path, const char* Access, void* Param);
    void(*Close)(HOBJ hFile);
    uint32(*Read)(HOBJ hFile, char* Buffer, uint32 BufferSize);
    uint32(*Write)(HOBJ hFile, const char* Buffer, uint32 BufferSize);
    uint64(*Tell)(HOBJ hFile);
    void(*Seek)(HOBJ hFile, int64 Offset, FileSeek_t Seek);
    void(*Sync)(HOBJ hFile);
    bool(*SyncEx)(HOBJ hFile, uint32 Timeout);
    void(*Flush)(HOBJ hFile);
    bool(*IsEof)(HOBJ hFile);
    uint64(*Size)(HOBJ hFile);
    FileStat_t(*Stat)(HOBJ hFile);
    int32(*Error)(HOBJ hFile);
    bool(*Abort)(HOBJ hFile);
    bool(*Exist)(FileSystem_t* Fs, const char* Path);
    bool(*Delete)(FileSystem_t* Fs, const char* Path);
};

struct File_t
{
    FileSystem_t* Fs;
};

bool FSysOpen(void);
void FSysClose(void);
bool FSysRegist(FileSystem_t* Fs, const char* Drive, const char* Name);
void FSysRemove(FileSystem_t* Fs);
FileSystem_t* FSysSearchByName(const char* Name);
FileSystem_t* FSysSearchByPath(const char* Path);
void FSysSetDefault(FileSystem_t* Fs);
FileSystem_t* FSysGetDefault(void);