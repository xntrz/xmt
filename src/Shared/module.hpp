#pragma once

#define WM_MODULE_REQUEST_LOCK      (WM_USER + 1)
#define WM_MODULE_REQUEST_UNLOCK    (WM_USER + 2)
#define WM_MODULE_ATTACH            (WM_USER + 3)
#define WM_MODULE_DETACH            (WM_USER + 4)

#define MODULE_MAX              (32)
#define MODULE_ENTRY_NAME       "RequestModuleDesc"
#define MODULE_DLGTYPE_MAIN     (0)
#define MODULE_DLGTYPE_SETTINGS (1)


struct ModuleDesc_t
{
    const char* Label;
    void(*FnInitialize)(HINSTANCE hInstance);
    void(*FnTerminate)(void);
    void(*FnAttach)(void);
    void(*FnDetach)(void);
    void(*FnLock)(void);
    void(*FnUnlock)(void);
    HWND(*FnRequestDlg)(HWND hWndParent, int32 Type);
};


typedef const ModuleDesc_t* (*RequestModuleDesc_t)(void);


#define INIT_MODULE(ModuleDesc)                                                 \
    extern "C" DLLEXPORT const ModuleDesc_t* RequestModuleDesc(void)            \
    {                                                                           \
        return &ModuleDesc;                                                     \
    }