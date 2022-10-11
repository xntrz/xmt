#include "DirWatch.hpp"
#include "IoService.hpp"


struct DirWatchAct_t
{
    OVERLAPPED Ovl;
    uint32 Bytes;
    char Buffer[4096];
};


struct DirWatch_t
{
    HANDLE hDir;
    TCHAR tszPath[MAX_PATH];
    std::atomic<int32> PauseLevel;
    DirWatchAct_t Act;
    DirWatchEventProc_t EventProc;
    DirWatchFilter_t FilterFlags;
    void* UserParam;
};


struct DirWatchContainer_t
{
    HOBJ hIoSvc;
    std::atomic<int32> RefCount;
};


static DirWatchContainer_t DirWatchContainer;


static DirWatchEvent_t DirWatchActionToEvent(uint32 FileAction)
{
    switch (FileAction)
    {
    case FILE_ACTION_ADDED:
        return DirWatchEvent_FileAdd;

    case FILE_ACTION_REMOVED:
        return DirWatchEvent_FileRemove;

    case FILE_ACTION_MODIFIED:
        return DirWatchEvent_FileModify;

    case FILE_ACTION_RENAMED_OLD_NAME:
        return DirWatchEvent_FileRenameOld;

    case FILE_ACTION_RENAMED_NEW_NAME:
        return DirWatchEvent_FileRenameNew;

    default:
        ASSERT(false);
        return DirWatchEvent_t(-1);
    };
};


static bool DirWatchStartListen(DirWatch_t* DirWatch)
{
    DirWatch->Act = { 0 };

    uint32 Flags = 0;
    BOOL bSubtree = FALSE;
    
    if (IS_FLAG_SET(DirWatch->FilterFlags, DirWatchFilter_ChangeFile))
        FLAG_SET(Flags, FILE_NOTIFY_CHANGE_FILE_NAME);
    
    if (IS_FLAG_SET(DirWatch->FilterFlags, DirWatchFilter_ChangeDir))
        FLAG_SET(Flags, FILE_NOTIFY_CHANGE_DIR_NAME);
    
    if (IS_FLAG_SET(DirWatch->FilterFlags, DirWatchFilter_ChangeAttr))
        FLAG_SET(Flags, FILE_NOTIFY_CHANGE_ATTRIBUTES);
    
    if (IS_FLAG_SET(DirWatch->FilterFlags, DirWatchFilter_ChangeSize))
        FLAG_SET(Flags, FILE_NOTIFY_CHANGE_SIZE);
    
    if (IS_FLAG_SET(DirWatch->FilterFlags, DirWatchFilter_ChangeWrite))
        FLAG_SET(Flags, FILE_NOTIFY_CHANGE_LAST_WRITE);
    
    if (IS_FLAG_SET(DirWatch->FilterFlags, DirWatchFilter_ChangeAccess))
        FLAG_SET(Flags, FILE_NOTIFY_CHANGE_LAST_ACCESS);
    
    if (IS_FLAG_SET(DirWatch->FilterFlags, DirWatchFilter_ChangeCreate))
        FLAG_SET(Flags, FILE_NOTIFY_CHANGE_CREATION);
    
    if (IS_FLAG_SET(DirWatch->FilterFlags, DirWatchFilter_ChangeSec))
        FLAG_SET(Flags, FILE_NOTIFY_CHANGE_SECURITY);

    if (IS_FLAG_SET(DirWatch->FilterFlags, DirWatchFilter_ChangeSubtree))
        bSubtree = TRUE;

    return (ReadDirectoryChangesW(
        DirWatch->hDir,
        DirWatch->Act.Buffer,
        COUNT_OF(DirWatch->Act.Buffer),
        bSubtree,
        Flags,
        NULL,
        &DirWatch->Act.Ovl,
        NULL) > 0);
};


static bool DirWatchIoComplete(uint32 ErrorCode, uint32 Bytes, void* CompletionKey, void* Act, void* Param)
{
	if (ErrorCode == ERROR_NOTIFY_CLEANUP)
		return true;

    if (ErrorCode != 0)
    {
        OUTPUTLN("Unexpected branch with error code %u", ErrorCode);
        return false;
    };

    DirWatch_t* DirWatch = (DirWatch_t*)CompletionKey;
    ASSERT(DirWatch);
    ASSERT(Bytes > 0);

    if (DirWatch->PauseLevel <= 0)
    {
        FILE_NOTIFY_INFORMATION* FileNotifyInfo = (FILE_NOTIFY_INFORMATION*)DirWatch->Act.Buffer;
        DirWatchEvent_t Event = DirWatchActionToEvent(FileNotifyInfo->Action);

        TCHAR* Filename = nullptr;
        int32 FilenameLen = 0;

#ifndef _UNICODE    
        char szTmp[MAX_PATH * 2];
        szTmp[0] = '\0';
        std::wcstombs(szTmp, FileNotifyInfo->FileName, COUNT_OF(szTmp));

        Filename = szTmp;
		FilenameLen = FileNotifyInfo->FileNameLength;
#else
        Filename = FileNotifyInfo->FileName;
        FilenameLen = FileNotifyInfo->FileNameLength;
#endif

        TCHAR tszFilenameBuff[MAX_PATH * 2];
		tszFilenameBuff[0] = TEXT('\0');

        int32 FilenameBuffLen = 0;

        if (IS_FLAG_SET(DirWatch->FilterFlags, DirWatchFilter_FullPath))
        {
            _tcscat(tszFilenameBuff, DirWatch->tszPath);
            _tcscat(tszFilenameBuff, TEXT("\\"));
        };        

        _tcscat(tszFilenameBuff, Filename);
        FilenameBuffLen = _tclen(tszFilenameBuff);

        DirWatch->EventProc(
            DirWatch,
            Event,
            tszFilenameBuff,
            FilenameBuffLen,
            DirWatch->UserParam
        );
    };

    DirWatchStartListen(DirWatch);

    return true;
};


void DirWatchInitialize(void)
{
    IoSvcCallbacks_t IoSvcCallbacks = {};
    IoSvcCallbacks.IoComplete = DirWatchIoComplete;
    
    DirWatchContainer.hIoSvc = IoSvcCreate("DirWatchIoSvc", 1, &IoSvcCallbacks);
    ASSERT(DirWatchContainer.hIoSvc != 0);
    
    DirWatchContainer.RefCount = 0;
};


void DirWatchTerminate(void)
{
    ASSERT(DirWatchContainer.RefCount == 0, "%d", DirWatchContainer.RefCount);
    
    if (DirWatchContainer.hIoSvc)
    {
        IoSvcDestroy(DirWatchContainer.hIoSvc);
        DirWatchContainer.hIoSvc = 0;
    };
};


HOBJ DirWatchCreate(const TCHAR* ptszPath, DirWatchEventProc_t EventProc, DirWatchFilter_t Filter, void * Param)
{
    ASSERT(ptszPath);
    ASSERT(EventProc);
    ASSERT(_tcslen(ptszPath) < COUNT_OF(DirWatch_t::tszPath));
    
    DirWatch_t* DirWatch = new DirWatch_t();
    if (!DirWatch)
        return 0;

    bool bResult = false;
    
    _tcscpy(DirWatch->tszPath, ptszPath);
    DirWatch->hDir = CreateFile(DirWatch->tszPath,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
	);

    if (DirWatch->hDir != INVALID_HANDLE_VALUE)
    {
        DirWatch->PauseLevel = 0;
        DirWatch->Act = { 0 };
        DirWatch->EventProc = EventProc;
        DirWatch->FilterFlags = Filter;
        DirWatch->UserParam = Param;

        if (IoSvcBind(DirWatchContainer.hIoSvc, DirWatch->hDir, DirWatch))
        {
            if (DirWatchStartListen(DirWatch))
                bResult = true;
            else
                OUTPUTLN("listen failed %u", GetLastError());
        }
        else
        {
            OUTPUTLN("bind failed %u", GetLastError());
        };
    };

    if (!bResult)
    {
        if (DirWatch->hDir)
        {
            CloseHandle(DirWatch->hDir);
            DirWatch->hDir = NULL;
        };

        delete DirWatch;
        DirWatch = nullptr;
    }
    else
    {
        ++DirWatchContainer.RefCount;
    };

    return DirWatch;
};


void DirWatchDestroy(HOBJ hDirWatch)
{
    DirWatch_t* DirWatch = (DirWatch_t*)hDirWatch;

    ASSERT(DirWatchContainer.RefCount > 0);
    --DirWatchContainer.RefCount;

    if (DirWatch->hDir)
    {
        CloseHandle(DirWatch->hDir);
        DirWatch->hDir = NULL;
    };

    delete DirWatch;
    DirWatch = nullptr;
};


int32 DirWatchPause(HOBJ hDirWatch)
{
    DirWatch_t* DirWatch = (DirWatch_t*)hDirWatch;
    
    return DirWatch->PauseLevel++;
};


int32 DirWatchResume(HOBJ hDirWatch)
{
    DirWatch_t* DirWatch = (DirWatch_t*)hDirWatch;
    
    ASSERT(DirWatch->PauseLevel > 0);
    return DirWatch->PauseLevel--;
};