#include "TvlDlg.hpp"
#include "TvlRc.hpp"
#include "TvlSettings.hpp"
#include "TvlObj.hpp"
#include "TvlResult.hpp"

#include "Shared/Common/Dlg.hpp"
#include "Shared/Common/Configuration.hpp"

#include "Utils/init.hpp"


static void TvlInitialize(HINSTANCE hInstance);
static void TvlTerminate(void);
static void TvlAttach(void);
static void TvlDetach(void);
static void TvlLock(void);
static void TvlUnlock(void);
static HWND TvlRequestDlg(HWND hWndParent, int32 Type);


static const ModuleDesc_t TvlModuleDesc =
{
    "Tvl",
    TvlInitialize,
    TvlTerminate,
    TvlAttach,
    TvlDetach,
    TvlLock,
    TvlUnlock,
    TvlRequestDlg,
};


INIT_MODULE(TvlModuleDesc);


struct TvlModule_t
{
    HINSTANCE hInstance;
    HWND hDlgInst;
};


static TvlModule_t TvlModule;


static void TvlInitialize(HINSTANCE hInstance)
{
    UtilsInitialize();

    TvlModule.hInstance = hInstance;
    CfgLoad(hInstance, TvlModuleDesc.Label, IDF_INI);
    TvlSettingsInitialize();
};


static void TvlTerminate(void)
{
    TvlSettingsTerminate();
    UtilsTerminate();
};


static void TvlAttach(void)
{
    CTvlResult::Instance().Attach();
};


static void TvlDetach(void)
{
    CTvlResult::Instance().Detach();
};


static void TvlLock(void)
{
    CTvlResult::Instance().Start();
    TvlObjStart();
};


static void TvlUnlock(void)
{
    TvlObjStop();
    CTvlResult::Instance().Stop();
};


static HWND TvlRequestDlg(HWND hWndParent, int32 Type)
{
    switch (Type)
    {
    case MODULE_DLGTYPE_MAIN:
        if (!TvlModule.hDlgInst)
        {
            TvlModule.hDlgInst = TvlDlgCreate(TvlModule.hInstance, hWndParent);
            ASSERT(TvlModule.hDlgInst);
        };
        return TvlModule.hDlgInst;

    case MODULE_DLGTYPE_SETTINGS:
        return NULL;

    default:
        return NULL;
    };
};