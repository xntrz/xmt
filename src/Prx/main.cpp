#include "PrxDlg.hpp"
#include "PrxRc.hpp"
#include "PrxSettings.hpp"
#include "PrxObj.hpp"
#include "PrxResult.hpp"

#include "Shared/Common/Dlg.hpp"
#include "Shared/Common/Configuration.hpp"

#include "Utils/Javascript/Jsvm.hpp"
#include "Utils/Http/HttpReq.hpp"
#include "Utils/init.hpp"


static void PrxInitialize(HINSTANCE hInstance);
static void PrxTerminate(void);
static void PrxAttach(void);
static void PrxDetach(void);
static void PrxLock(void);
static void PrxUnlock(void);
static HWND PrxRequestDlg(HWND hWndParent, int32 Type);


static const ModuleDesc_t PrxModuleDesc =
{
    "Prx",
    PrxInitialize,
    PrxTerminate,
    PrxAttach,
    PrxDetach,
    PrxLock,
    PrxUnlock,
    PrxRequestDlg,
};


INIT_MODULE(PrxModuleDesc);


struct PrxModule_t
{
    HINSTANCE hInstance;
    HWND hDlgInst;
};


static PrxModule_t PrxModule;


static void PrxInitialize(HINSTANCE hInstance)
{
    UtilsInitialize();
    
    PrxModule.hInstance = hInstance;
    CfgLoad(hInstance, PrxModuleDesc.Label, IDF_INI);
    PrxSettingsInitialize();
};


static void PrxTerminate(void)
{
    PrxSettingsTerminate();
    
    UtilsTerminate();
};


static void PrxAttach(void)
{
    CPrxResult::Instance().Attach();
};


static void PrxDetach(void)
{
    CPrxResult::Instance().Detach();
};


static void PrxLock(void)
{
    CPrxResult::Instance().Start();
    PrxObjStart();
};


static void PrxUnlock(void)
{
    PrxObjStop();
    CPrxResult::Instance().Stop();
};


static HWND PrxRequestDlg(HWND hWndParent, int32 Type)
{
    switch (Type)
    {
    case MODULE_DLGTYPE_MAIN:
        if (!PrxModule.hDlgInst)
        {
            PrxModule.hDlgInst = PrxDlgCreate(PrxModule.hInstance, hWndParent);
            ASSERT(PrxModule.hDlgInst);
        };
        return PrxModule.hDlgInst;

    case MODULE_DLGTYPE_SETTINGS:
        return NULL;

    default:
        return NULL;
    };
};