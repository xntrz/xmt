#include "FsPhy.hpp"
#include "Fs.hpp"

#define PHYFILE_POOL


static_assert(FileSeek_Begin == FILE_BEGIN, "update me");
static_assert(FileSeek_Current == FILE_CURRENT, "update me");
static_assert(FileSeek_End == FILE_END, "update me");


struct PhyFile_t
{
    File_t BaseFile;
    void* Handle;
    char Path[MAX_PATH];
    bool FlagAppend;
    bool FlagTextMode;  // TODO text mode
#ifdef PHYFILE_POOL
    bool FlagAlloc;
#endif
};


struct PhyFileContainer_t
{
    FileSystem_t Fs;
    std::atomic<int32> FileOpenCount;
#ifdef PHYFILE_POOL
    PhyFile_t FilePool[8];
    std::mutex Mutex;
#endif
};


static PhyFileContainer_t PhyFileContainer;


static PhyFile_t* PhyFileAlloc(void);
static void PhyFileFree(PhyFile_t* PhyFile);
static HOBJ PhyFileOpen(FileSystem_t* Fs, const char* Path, const char* Access, void* Param);
static void PhyFileClose(HOBJ hFile);
static uint32 PhyFileRead(HOBJ hFile, char* Buffer, uint32 BufferSize);
static uint32 PhyFileWrite(HOBJ hFile, const char* Buffer, uint32 BufferSize);
static uint64 PhyFileTell(HOBJ hFile);
static void PhyFileSeek(HOBJ hFile, int64 Offset, FileSeek_t Seek);
static void PhyFileSync(HOBJ hFile);
static bool PhyFileSyncEx(HOBJ hFile, uint32 Timeout);
static void PhyFileFlush(HOBJ hFile);
static bool PhyFileIsEof(HOBJ hFile);
static uint64 PhyFileSize(HOBJ hFile);
static FileStat_t PhyFileStat(HOBJ hFile);
static int32 PhyFileError(HOBJ hFile);
static bool PhyFileAbort(HOBJ hFile);
static bool PhyFileExist(FileSystem_t* Fs, const char* Path);
static bool PhyFileDelete(FileSystem_t* Fs, const char* Path);


static PhyFile_t* PhyFileAlloc(void)
{
    PhyFile_t* PhyFile = nullptr;
#ifdef PHYFILE_POOL
    {
        std::lock_guard<std::mutex> Lock(PhyFileContainer.Mutex);
        for (int32 i = 0; i < COUNT_OF(PhyFileContainer.FilePool); ++i)
        {
            if (!PhyFileContainer.FilePool[i].FlagAlloc)
            {
                PhyFile = &PhyFileContainer.FilePool[i];
                PhyFile->FlagAlloc = true;
                break;
            };
        };
    }
#else
    PhyFile = new PhyFile_t();
#endif

    if (PhyFile)
        ++PhyFileContainer.FileOpenCount;

    return PhyFile;
};


static void PhyFileFree(PhyFile_t* PhyFile)
{
    ASSERT(PhyFileContainer.FileOpenCount > 0);
    --PhyFileContainer.FileOpenCount;

#ifdef PHYFILE_POOL
    {
        std::lock_guard<std::mutex> Lock(PhyFileContainer.Mutex);
        PhyFile->FlagAlloc = false;        
    }
#else
    delete PhyFile;
#endif
};


static HOBJ PhyFileOpen(FileSystem_t* Fs, const char* Path, const char* Access, void* Param)
{
    PhyFile_t* PhyFile = PhyFileAlloc();
    if (!PhyFile)
        return 0;

    uint32 OpenDispFlags = 0;
    uint32 OpenAccessFlags = 0;
    uint32 OpenAttrFlags = 0;

    bool FlagExtended = false;
    bool FlagTextMode = true;

    if (std::strstr(Access, "+"))
        FlagExtended = true;

    if (std::strstr(Access, "r"))
    {
        //
        //  Action if file already exists: read from start
        //  Action if file does not exist: return NULL and set error
        //  Same for extended
        //
        OpenAccessFlags |= GENERIC_READ;
        OpenDispFlags = OPEN_EXISTING;
        if (FlagExtended)
            OpenAccessFlags |= GENERIC_WRITE;
    };

    if (std::strstr(Access, "w"))
    {
        //
        //  Action if file already exists: destroy contents	
        //  Action if file does not exist: create new
        //  Same for extended
        //
        OpenAccessFlags |= GENERIC_WRITE;
        OpenDispFlags = CREATE_ALWAYS;
        if (FlagExtended)
            OpenAccessFlags |= GENERIC_READ;
    };
	
    if (std::strstr(Access, "a"))
    {
        //
        //  Action if file already exists: write to end	
        //  Action if file does not exist: create new
        //  Same for extended
        //
        PhyFile->FlagAppend = true;
        OpenAccessFlags = GENERIC_WRITE;
        OpenDispFlags = OPEN_EXISTING;
    };

    if (std::strstr(Access, "b"))
    {
        //
        //  TODO text mode
        //
        FlagTextMode = false;
    };

    PhyFile->Handle = CreateFileA(
        Path,
        OpenAccessFlags,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OpenDispFlags,
        OpenAttrFlags,
        NULL
    );
    if (PhyFile->Handle != INVALID_HANDLE_VALUE)
    {
        if (PhyFile->FlagAppend)
            PhyFileSeek(PhyFile, 0, FileSeek_End);
    
        PhyFile->FlagTextMode = FlagTextMode;
    }
    else
    {
        OUTPUTLN("open failed path \"%s\", access \"%s\", error code %u", Path, Access, GetLastError());
        PhyFileFree(PhyFile);
        PhyFile = 0;
    };
    
    return PhyFile;
};


static void PhyFileClose(HOBJ hFile)
{
    PhyFile_t* PhyFile = (PhyFile_t*)hFile;

    PhyFile->FlagTextMode = false;
    PhyFile->FlagAppend = false;
    
    if (PhyFile->Handle)
    {
        CloseHandle(PhyFile->Handle);
        PhyFile->Handle = 0;
    };

    PhyFileFree(PhyFile);
};


static uint32 PhyFileRead(HOBJ hFile, char* Buffer, uint32 BufferSize)
{
    if (!Buffer || !BufferSize)
        return 0;

    PhyFile_t* PhyFile = (PhyFile_t*)hFile;

    uint32 Readed = 0;
    if (!ReadFile(PhyFile->Handle, Buffer, BufferSize, LPDWORD(&Readed), NULL))
        OUTPUTLN("phy file read failed %u", GetLastError());

    return Readed;
};


static uint32 PhyFileWrite(HOBJ hFile, const char* Buffer, uint32 BufferSize)
{
    PhyFile_t* PhyFile = (PhyFile_t*)hFile;
    uint32 Written = 0;

    if (PhyFile->FlagAppend)
        PhyFileSeek(PhyFile, 0, FileSeek_End);

    if (!WriteFile(PhyFile->Handle, Buffer, BufferSize, LPDWORD(&Written), NULL))
        OUTPUTLN("phy file write failed %u", GetLastError());

    return Written;
};


static uint64 PhyFileTell(HOBJ hFile)
{
    PhyFile_t* PhyFile = (PhyFile_t*)hFile;
    LARGE_INTEGER PosPrev = { 0 };

    if(!SetFilePointerEx(PhyFile->Handle, { 0 }, &PosPrev, FileSeek_Current))
        OUTPUTLN("phy file tell failed %u", GetLastError());
    
    return uint64(PosPrev.QuadPart);
};


static void PhyFileSeek(HOBJ hFile, int64 Offset, FileSeek_t Seek)
{
    PhyFile_t* PhyFile = (PhyFile_t*)hFile;
    LARGE_INTEGER DistMove = { 0 };
    
    DistMove.QuadPart = Offset;
    if (!SetFilePointerEx(PhyFile->Handle, DistMove, NULL, Seek))
        OUTPUTLN("phy file seek failed %u", GetLastError());
};


static void PhyFileSync(HOBJ hFile)
{
    ;
};


static bool PhyFileSyncEx(HOBJ hFile, uint32 Timeout)
{
    return true;
};


static void PhyFileFlush(HOBJ hFile)
{
    PhyFile_t* PhyFile = (PhyFile_t*)hFile;

    if (!FlushFileBuffers(PhyFile->Handle))
        OUTPUTLN("phy file flush failed %u", GetLastError());
};


static bool PhyFileIsEof(HOBJ hFile)
{
    return (PhyFileTell(hFile) == PhyFileSize(hFile));
};


static uint64 PhyFileSize(HOBJ hFile)
{
    PhyFile_t* PhyFile = (PhyFile_t*)hFile;
    LARGE_INTEGER Size = { 0 };

    if (!GetFileSizeEx(PhyFile->Handle, &Size))
        OUTPUTLN("phy file size failed %u", GetLastError());

    return uint64(Size.QuadPart);
};


static FileStat_t PhyFileStat(HOBJ hFile)
{
    return FileStat_Ready;
};


static int32 PhyFileError(HOBJ hFile)
{
    return GetLastError();
};


static bool PhyFileAbort(HOBJ hFile)
{
    return true;
};


static bool PhyFileExist(FileSystem_t* Fs, const char* Path)
{
    DWORD Attributes = 0;

    if ((Attributes = GetFileAttributesA(Path)) == INVALID_FILE_ATTRIBUTES)
        Attributes = 0;

    return IS_FLAG_SET(Attributes, FILE_ATTRIBUTE_DIRECTORY);
};


static bool PhyFileDelete(FileSystem_t* Fs, const char* Path)
{
    return (DeleteFileA(Path) > 0);
};


bool PhyFsOpen(const char* Path)
{
    FileSystem_t* PhyFs = &PhyFileContainer.Fs;
    PhyFs->Open     = PhyFileOpen;
    PhyFs->Close    = PhyFileClose;
    PhyFs->Delete   = PhyFileDelete;
    PhyFs->Read     = PhyFileRead;
    PhyFs->Write    = PhyFileWrite;
    PhyFs->Tell     = PhyFileTell;
    PhyFs->Seek     = PhyFileSeek;
    PhyFs->Sync     = PhyFileSync;
    PhyFs->SyncEx   = PhyFileSyncEx;
    PhyFs->Flush    = PhyFileFlush;
    PhyFs->IsEof    = PhyFileIsEof;
    PhyFs->Size     = PhyFileSize;
    PhyFs->Stat     = PhyFileStat;
    PhyFs->Error    = PhyFileError;
    PhyFs->Abort    = PhyFileAbort;
    PhyFs->Exist    = PhyFileExist;

    return FSysRegist(PhyFs, Path, "Phy");
};


void PhyFsClose(void)
{
    ASSERT(PhyFileContainer.FileOpenCount == 0);
    
    FileSystem_t* PhyFs = &PhyFileContainer.Fs;
    FSysRemove(PhyFs);
};