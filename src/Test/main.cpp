

static void TestInitialize(HINSTANCE hInstance);
static void TestTerminate(void);
static void TestAttach(void);
static void TestDetach(void);
static void TestLock(void);
static void TestUnlock(void);
static HWND TestRequestDlg(HWND hWndParent, int32 Type);


static const ModuleDesc_t TestModuleDesc =
{
    "Test",
    TestInitialize,
    TestTerminate,
    TestAttach,
    TestDetach,
    TestLock,
    TestUnlock,
    TestRequestDlg,
};


INIT_MODULE(TestModuleDesc);


static void TestInitialize(HINSTANCE hInstance)
{

};


static void TestTerminate(void)
{
    ;
};


static void TestAttach(void)
{
    ;
};


static void TestDetach(void)
{
    ;
};


static void TestLock(void)
{
    ;
};


static void TestUnlock(void)
{
    ;
};


static HWND TestRequestDlg(HWND hWndParent, int32 Type)
{
    return NULL;
};