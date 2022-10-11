#include "BngDlg.hpp"
#include "BngRc.hpp"
#include "BngSettings.hpp"
#include "BngObj.hpp"
#include "BngResult.hpp"

#include "Shared/Common/Dlg.hpp"
#include "Shared/Common/Configuration.hpp"

#include "Utils/init.hpp"


static void BngInitialize(HINSTANCE hInstance);
static void BngTerminate(void);
static void BngAttach(void);
static void BngDetach(void);
static void BngLock(void);
static void BngUnlock(void);
static HWND BngRequestDlg(HWND hWndParent, int32 Type);


static const ModuleDesc_t BngModuleDesc =
{
    "Bng",
    BngInitialize,
    BngTerminate,
    BngAttach,
    BngDetach,
    BngLock,
    BngUnlock,
    BngRequestDlg,
};


INIT_MODULE(BngModuleDesc);


struct BngModule_t
{
    HINSTANCE hInstance;
    HWND hDlgInst;
};


static BngModule_t BngModule;


static void BngInitialize(HINSTANCE hInstance)
{
    UtilsInitialize();

    BngModule.hInstance = hInstance;
    CfgLoad(hInstance, BngModuleDesc.Label, IDF_INI);
    BngSettingsInitialize();
};


static void BngTerminate(void)
{
    BngSettingsTerminate();
    UtilsTerminate();
};


static void BngAttach(void)
{
    CBngResult::Instance().Attach();
};


static void BngDetach(void)
{
    CBngResult::Instance().Detach();
};


static void BngLock(void)
{
    CBngResult::Instance().Start();
    BngObjStart();
};


static void BngUnlock(void)
{
    BngObjStop();
    CBngResult::Instance().Stop();
};


static HWND BngRequestDlg(HWND hWndParent, int32 Type)
{
    switch (Type)
    {
    case MODULE_DLGTYPE_MAIN:
        if (!BngModule.hDlgInst)
        {
            BngModule.hDlgInst = BngDlgCreate(BngModule.hInstance, hWndParent);
            ASSERT(BngModule.hDlgInst);
        };
        return BngModule.hDlgInst;

    case MODULE_DLGTYPE_SETTINGS:
        return NULL;

    default:
        return NULL;
    };
};