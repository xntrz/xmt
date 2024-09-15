#include "nk_font.hpp"

#include "../File/File.hpp"
#include "../File/FsRc.hpp"


static HANDLE s_hFont = NULL;
static int32 s_fontHeight = 16;


bool nk_font_install(HINSTANCE hInstance, int32 rcid)
{
    bool bResult = false;

    HOBJ hFile = FileOpen(RcFsBuildPath(hInstance, rcid), "rb");
    if (hFile)
    {
        uint32 fsize = uint32(FileSize(hFile));

        std::vector<char> buffer;
        buffer.resize(std::size_t(fsize));

        uint32 bytes = FileRead(hFile, &buffer[0], buffer.size());
        ASSERT(bytes == fsize);

        DWORD Count = 0;
        s_hFont = ::AddFontMemResourceEx(&buffer[0], buffer.size(), 0, &Count);
        if (s_hFont == NULL)
            OUTPUTLN("app font loading failed (%" PRIu32 ")", ::GetLastError());

        FileClose(hFile);
    };

    return bResult;
};


void nk_font_uninstall()
{
    if (s_hFont != NULL)
    {
        RemoveFontMemResourceEx(s_hFont);        
        s_hFont = NULL;
    };
};


/*DLLSHARED*/ void nk_font_set_height(int32 h)
{
    s_fontHeight = h;
};


/*DLLSHARED*/ const char* nk_font_get_name()
{
    return (s_hFont ? "Segoe UI" : "Arial");
};


/*DLLSHARED*/ int32 nk_font_get_height()
{
    return s_fontHeight;
};