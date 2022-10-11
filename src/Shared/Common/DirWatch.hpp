#pragma once

enum DirWatchEvent_t
{
    DirWatchEvent_FileAdd = 0,
    DirWatchEvent_FileRemove,
    DirWatchEvent_FileModify,
    DirWatchEvent_FileRenameOld,
    DirWatchEvent_FileRenameNew,
};

enum DirWatchFilter_t
{
    DirWatchFilter_ChangeFile   = BIT(0),
    DirWatchFilter_ChangeDir    = BIT(1),
    DirWatchFilter_ChangeAttr   = BIT(2),
    DirWatchFilter_ChangeSize   = BIT(3),
    DirWatchFilter_ChangeWrite  = BIT(4),
    DirWatchFilter_ChangeAccess = BIT(5),
    DirWatchFilter_ChangeCreate = BIT(6),
    DirWatchFilter_ChangeSec    = BIT(7),
    DirWatchFilter_ChangeSubtree= BIT(8),
    DirWatchFilter_FullPath     = BIT(9),

    DirWatchFilter_Default      =   DirWatchFilter_ChangeFile |
                                    DirWatchFilter_ChangeDir |
                                    DirWatchFilter_ChangeSubtree | 
                                    DirWatchFilter_FullPath,
};

typedef void(*DirWatchEventProc_t)(
    HOBJ            hDirWatch,
    DirWatchEvent_t Event,
    const TCHAR*    ptszFilename,
    int32           FilenameLen,
    void*           Param
);

DLLSHARED void DirWatchInitialize(void);
DLLSHARED void DirWatchTerminate(void);
DLLSHARED HOBJ DirWatchCreate(const TCHAR* ptszPath, DirWatchEventProc_t EventProc, DirWatchFilter_t Filter = DirWatchFilter_Default, void* Param = nullptr);
DLLSHARED void DirWatchDestroy(HOBJ hDirWatch);
DLLSHARED int32 DirWatchPause(HOBJ hDirWatch);
DLLSHARED int32 DirWatchResume(HOBJ hDirWatch);