#include "FsRc.hpp"
#include "Fs.hpp"

#pragma warning(disable : 4477)

#define RCFILE_DEF_TYPE "RAWFILE"
#define RCFILE_NAME     "Rc"
#define RCFILE_PATH     "rc:"

#define RCFILE_POOL


struct RcFile_t
{
    File_t BaseFile;
    void* Handle;
    void* Buffer;
    uint64 BufferSize;
    uint64 Position;
    bool FlagTextMode;  // TODO text mode
#ifdef RCFILE_POOL
    bool FlagAlloc;
#endif
};


struct RcFileContainer_t
{
    FileSystem_t Fs;
    std::atomic<int32> FileOpenCount;    
#ifdef RCFILE_POOL
    RcFile_t FilePool[8];
    std::mutex Mutex;
#endif
};


static RcFileContainer_t RcFileContainer;


static RcFile_t* RcFileAlloc(void);
static void RcFileFree(RcFile_t* RcFile);
static HOBJ RcFileOpen(FileSystem_t* Fs, const char* Path, const char* Access, void* Param);
static void RcFileClose(HOBJ hFile);
static uint32 RcFileRead(HOBJ hFile, char* Buffer, uint32 BufferSize);
static uint32 RcFileWrite(HOBJ hFile, const char* Buffer, uint32 BufferSize);
static uint64 RcFileTell(HOBJ hFile);
static void RcFileSeek(HOBJ hFile, int64 Offset, FileSeek_t Seek);
static void RcFileSync(HOBJ hFile);
static bool RcFileSyncEx(HOBJ hFile, uint32 Timeout);
static void RcFileFlush(HOBJ hFile);
static bool RcFileIsEof(HOBJ hFile);
static uint64 RcFileSize(HOBJ hFile);
static FileStat_t RcFileStat(HOBJ hFile);
static int32 RcFileError(HOBJ hFile);
static bool RcFileAbort(HOBJ hFile);
static bool RcFileExist(FileSystem_t* Fs, const char* Path);
static bool RcFileDelete(FileSystem_t* Fs, const char* Path);


static const char* RcFileDefTypeIdNameTable[] =
{
    "RT_CURSOR",
    "RT_BITMAP",
    "RT_ICON",
    "RT_MENU",
    "RT_DIALOG",
    "RT_STRING",
    "RT_FONTDIR",
    "RT_FONT",
    "RT_ACCELERATOR",
    "RT_RCDATA",
    "RT_MESSAGETABLE",
};


static bool RcFileIsDefTypeId(const char* DefTypeId)
{
    return ((uint32(DefTypeId) >= uint32(RT_CURSOR)) &&
            (uint32(DefTypeId) <= uint32(RT_MESSAGETABLE)));
};


static bool RcFileIsDefName(const char* DefName)
{
    for (int32 i = 0; i < COUNT_OF(RcFileDefTypeIdNameTable); ++i)
    {
        if (!std::strcmp(DefName, RcFileDefTypeIdNameTable[i]))
            return true;
    };

    return false;
};


static const char* RcFileDefTypeIdToDefName(const char* DefTypeId)
{
    int32 Index = int32(DefTypeId) - 1;
    ASSERT(Index >= 0 && Index < COUNT_OF(RcFileDefTypeIdNameTable));
    return RcFileDefTypeIdNameTable[Index];
};


static const char* RcFileDefNameToDefTypeId(const char* DefName)
{
    for (int32 i = 0; i < COUNT_OF(RcFileDefTypeIdNameTable); ++i)
    {
        if (!std::strcmp(DefName, RcFileDefTypeIdNameTable[i]))
            return (const char*)i + 1;
    };

    return nullptr;
};


static RcFile_t* RcFileAlloc(void)
{
    RcFile_t* RcFile = nullptr;
#ifdef RCFILE_POOL
    {
        std::lock_guard<std::mutex> Lock(RcFileContainer.Mutex);
        for (int32 i = 0; i < COUNT_OF(RcFileContainer.FilePool); ++i)
        {
            if (!RcFileContainer.FilePool[i].FlagAlloc)
            {
                RcFile = &RcFileContainer.FilePool[i];
                RcFile->FlagAlloc = true;
                break;
            };
        };
    }
#else
    RcFile = new RcFile_t();
#endif

    if (RcFile)
        ++RcFileContainer.FileOpenCount;

    return RcFile;
};


static void RcFileFree(RcFile_t* RcFile)
{
    ASSERT(RcFileContainer.FileOpenCount > 0);
    --RcFileContainer.FileOpenCount;

#ifdef RCFILE_POOL
    {
        std::lock_guard<std::mutex> Lock(RcFileContainer.Mutex);
        RcFile->FlagAlloc = false;
    }
#else
    delete RcFile;
#endif
};


static HOBJ RcFileOpen(FileSystem_t* Fs, const char* Path, const char* Access, void* Param)
{
    if (!std::strstr(Access, "r"))
        return 0;    

    const char* RcTypePtr = nullptr;
    HMODULE hModule = NULL;
    int32 RcId = 0;    
    char RcType[256];
    RcType[0] = '\0';
    
    int32 ScanRes = std::sscanf(Path, "%*[^:]:%u/%d.%s", &hModule, &RcId, &RcType[0]);
    if (ScanRes != 3)
        return 0;
    
    if (RcFileIsDefName(RcType))
        RcTypePtr = RcFileDefNameToDefTypeId(RcType);
    else
        RcTypePtr = RcType;

    RcFile_t* RcFile = RcFileAlloc();
    if (RcFile)
    {
        bool bResult = false;
        HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(RcId), RcTypePtr);
        if (hRes)
        {
            RcFile->Handle = LoadResource(hModule, hRes);
            if (RcFile->Handle)
            {
                RcFile->Buffer = LockResource(RcFile->Handle);
                RcFile->BufferSize = SizeofResource(hModule, hRes);
                RcFile->FlagTextMode = false;
                
                ASSERT(RcFile->Buffer);
                ASSERT(RcFile->BufferSize > 0);

                bResult = true;
            };
        };

        if (!bResult)
        {
            FreeResource(RcFile->Handle);
            RcFileFree(RcFile);
            RcFile = nullptr;
        };
    };

    return RcFile;
};


static void RcFileClose(HOBJ hFile)
{
    RcFile_t* RcFile = (RcFile_t*)hFile;

    if (RcFile->Handle != NULL)
    {
        FreeResource(RcFile->Handle);
        RcFile->Handle = NULL;
        RcFile->Buffer = nullptr;
        RcFile->BufferSize = 0;
        RcFile->Position = 0;        
    };
    
    RcFileFree(RcFile);
};


static uint32 RcFileRead(HOBJ hFile, char* Buffer, uint32 BufferSize)
{
    RcFile_t* RcFile = (RcFile_t*)hFile;
    
    if (RcFileIsEof(RcFile))
        return 0;
    
    uint32 Read = Min(BufferSize, uint32(RcFile->BufferSize - RcFile->Position));
    std::memcpy(Buffer, &((char*)RcFile->Buffer)[RcFile->Position], Read);
    RcFile->Position += uint64(Read);
    
    return Read;
};


static uint32 RcFileWrite(HOBJ hFile, const char* Buffer, uint32 BufferSize)
{
    OUTPUTLN("write attempt to rc file");
    return 0;
};


static uint64 RcFileTell(HOBJ hFile)
{
    RcFile_t* RcFile = (RcFile_t*)hFile;

    return RcFile->Position;    
};


static void RcFileSeek(HOBJ hFile, int64 Offset, FileSeek_t Seek)
{
    RcFile_t* RcFile = (RcFile_t*)hFile;

    int64 Pos = int64(RcFile->Position);
    switch (Seek)
    {
    case FileSeek_Begin:
        {
            Pos = uint64(Offset);
        }
        break;

    case FileSeek_Current:
        {
            Pos += Offset;
        }
        break;

    case FileSeek_End:
        {
            Pos = (int64(RcFile->BufferSize) + Offset);
        }
        break;
    };

    RcFile->Position = Pos;
    ASSERT((RcFile->Position >= 0) && (RcFile->Position <= RcFile->BufferSize));
    RcFile->Position = Clamp(RcFile->Position, 0ull, RcFile->BufferSize);    
};


static void RcFileSync(HOBJ hFile)
{
    ;
};


static bool RcFileSyncEx(HOBJ hFile, uint32 Timeout)
{
    return true;
};


static void RcFileFlush(HOBJ hFile)
{
    ;
};


static bool RcFileIsEof(HOBJ hFile)
{
    return (RcFileTell(hFile) == RcFileSize(hFile));
};


static uint64 RcFileSize(HOBJ hFile)
{
    RcFile_t* RcFile = (RcFile_t*)hFile;

    return RcFile->BufferSize;
};


static FileStat_t RcFileStat(HOBJ hFile)
{
    return FileStat_Ready;
};


static int32 RcFileError(HOBJ hFile)
{
    return 0;
};


static bool RcFileAbort(HOBJ hFile)
{
    return true;
};


static bool RcFileExist(FileSystem_t* Fs, const char* Path)
{
    bool bResult = false;
    HOBJ hRcFile = RcFileOpen(Fs, Path, "rb", nullptr);
    if (hRcFile)
    {
        bResult = true;
        RcFileClose(hRcFile);
    };

    return bResult;
};


static bool RcFileDelete(FileSystem_t* Fs, const char* Path)
{
    return false;
};


bool RcFsOpen(void)
{
    FileSystem_t* RcFs = &RcFileContainer.Fs;
    RcFs->Open      = RcFileOpen;
    RcFs->Close     = RcFileClose;
    RcFs->Delete    = RcFileDelete;
    RcFs->Read      = RcFileRead;
    RcFs->Write     = RcFileWrite;
    RcFs->Tell      = RcFileTell;
    RcFs->Seek      = RcFileSeek;
    RcFs->Sync      = RcFileSync;
    RcFs->SyncEx    = RcFileSyncEx;
    RcFs->Flush     = RcFileFlush;
    RcFs->IsEof     = RcFileIsEof;
    RcFs->Size      = RcFileSize;
    RcFs->Stat      = RcFileStat;
    RcFs->Error     = RcFileError;
    RcFs->Abort     = RcFileAbort;
    RcFs->Exist     = RcFileExist;

    return FSysRegist(RcFs, RCFILE_PATH, RCFILE_NAME);
};


void RcFsClose(void)
{
    ASSERT(RcFileContainer.FileOpenCount == 0);

    FileSystem_t* RcFs = &RcFileContainer.Fs;
    FSysRemove(RcFs);
};


const char* RcFsBuildPath(HMODULE hModule, int32 RcId)
{
    return RcFsBuildPathEx(hModule, RcId, RCFILE_DEF_TYPE);    
};


const char* RcFsBuildPathEx(HMODULE hModule, int32 RcId, const char* RcType)
{
    thread_local static char PathBuffer[MAX_PATH];
    PathBuffer[0] = '\0';

    if (RcFileIsDefTypeId(RcType))
        RcType = RcFileDefTypeIdToDefName(RcType);

    std::sprintf(PathBuffer, "%s%u/%d.%s", RCFILE_PATH, hModule, RcId, RcType);

    return PathBuffer;
};