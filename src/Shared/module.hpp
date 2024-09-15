#pragma once

#define MODULE_MAX              (32)
#define MODULE_ENTRY_NAME       "ModuleEvtProc"


#define MODULE_EVT_STARTUP  (0)
#define MODULE_EVT_SHUTDOWN (1)
#define MODULE_EVT_ATTACH   (2)
#define MODULE_EVT_DETACH   (3)
#define MODULE_EVT_INFO     (5)


struct MODULE_EVT
{
    int id;
    union
    {
        struct
        {
            HINSTANCE hInstance;
        } shutdown, startup;

        struct
        {
            struct nkgdi_window* wnd;
            struct nk_context* ctx;
            bool ret;
        } draw;

        struct
        {
            const char* tag;
            const char* title;
            const char* description;
            int icoid;
            bool(*ui_proc)(struct nkgdi_window* wnd, struct nk_context* ctx, bool ret);
        } info;
    } param;
};


using MODULE_EVT_PROC = void(*)(MODULE_EVT&);
