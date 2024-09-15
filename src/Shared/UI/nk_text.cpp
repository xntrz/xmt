#include "nk_text.hpp"

#include "../File/File.hpp"
#include "../File/FsRc.hpp"


extern void nkgdi_window_locale_changed();


struct nk_text_locale_info
{
    nk_text_locale locale;
    WORD langid;
    WORD sublangid;
    std::string label;

    inline nk_text_locale_info(nk_text_locale _locale, WORD _langid, WORD _sublangid, const std::string& _label)
        : locale(_locale), langid(_langid), sublangid(_sublangid), label(_label) {};
};


struct nk_text
{
    std::string path;
    std::vector<char> data;
    std::vector<std::size_t> offsets;

    inline nk_text()
        : path(), data(), offsets() {};
};


struct nk_text_container
{
    nk_text_container();
    bool path_push(const std::string& path, bool locale);
    void path_pop();
    void set_locale(nk_text_locale locale);
    bool load(const std::string& path, const void* data, std::size_t size);
    bool load_rc(const std::string& path, HMODULE hMod, int32 rcid);
    void unload(const std::string& path);
    const char* get_text(std::size_t id) const;

private:
    std::list<std::shared_ptr<struct nk_text>> m_textList;
    std::shared_ptr<struct nk_text> m_textCurrent;
    std::vector<nk_text_locale_info> m_textLocaleInfo;
    std::stack<std::shared_ptr<struct nk_text>> m_textStack;
    nk_text_locale_info* m_textLocaleInfoCurrent;
};


nk_text_container::nk_text_container()
: m_textList()
, m_textCurrent(nullptr)
, m_textLocaleInfo()
, m_textLocaleInfoCurrent(nullptr)
{
    m_textLocaleInfo.push_back(nk_text_locale_info(nk_text_locale_en, LANG_ENGLISH, SUBLANG_ENGLISH_US, "_en"));
    m_textLocaleInfo.push_back(nk_text_locale_info(nk_text_locale_ru, LANG_RUSSIAN, SUBLANG_RUSSIAN_RUSSIA, "_ru"));
};


bool nk_text_container::path_push(const std::string& path, bool locale)
{
    std::string realpath = (path + (locale ? m_textLocaleInfoCurrent->label : ""));

    auto it = std::find_if(m_textList.begin(), m_textList.end(), [&](std::shared_ptr<struct nk_text>& ptr) {
        return (ptr->path == realpath);
    });

    if (it != m_textList.end()) {
        m_textStack.push(*it);
        return true;
    };

    return false;
};


void nk_text_container::path_pop()
{
    m_textStack.pop();
};


void nk_text_container::set_locale(nk_text_locale locale)
{
    std::vector<nk_text_locale_info>::iterator it;

    if (m_textLocaleInfoCurrent && (m_textLocaleInfoCurrent->locale == locale))
        return;

    if (locale == nk_text_locale_sysdefault) {
        LANGID deflangid = GetUserDefaultLangID();
        it = std::find_if(m_textLocaleInfo.begin(), m_textLocaleInfo.end(), [&deflangid](const nk_text_locale_info& info) {
            return (info.langid == PRIMARYLANGID(deflangid)) && (info.sublangid == SUBLANGID(deflangid));
        });
        if (it == m_textLocaleInfo.end())
            set_locale(nk_text_locale_default);
    }
    else {
        it = std::find_if(m_textLocaleInfo.begin(), m_textLocaleInfo.end(), [&locale](const nk_text_locale_info& info) {
            return (info.locale == locale);
        });
    };

    if (it != m_textLocaleInfo.end()) {
        m_textLocaleInfoCurrent = &(*it);
        SetThreadUILanguage(MAKELANGID(it->langid, it->sublangid));
        nkgdi_window_locale_changed();
    };
};


bool nk_text_container::load(const std::string& path, const void* data, std::size_t size)
{
    /* check if already loaded at this path */
    auto it = std::find_if(m_textList.begin(), m_textList.end(), [&](std::shared_ptr<struct nk_text>& ptr) {
        return (ptr->path == path);
    });

    if (it != m_textList.end())
        return false;

    /* init text container */
    std::shared_ptr<nk_text> text = std::make_shared<nk_text>();
    if (text)
    {
        text->path = path;
        text->data = std::vector<char>(
            reinterpret_cast<const char*>(data),
            reinterpret_cast<const char*>(data) + size
        );
        text->data.push_back('\0');


        /* parse data for strings */
        std::array<int32, 256 * 256> offsetTbl;
        std::size_t offsetNum = 0u;
        std::size_t offset = 0u;

        for (auto& b : text->data)
        {
            if (b == '\n' || (&b == &text->data.back())) {
                offsetTbl[offsetNum++] = offset;
                offset = (&b - &text->data[0]) + 1u;
            };

            if (b == '\r' || b == '\n') {
                b = '\0';
            };
        };

        /* prealloc space & copy offsets */
        text->offsets.reserve(offsetNum);
        std::copy(
            offsetTbl.begin(),
            offsetTbl.begin() + offsetNum,
            std::back_inserter(text->offsets)
        );

        m_textList.push_back(text);
        return true;
    };

    return false;
};


bool nk_text_container::load_rc(const std::string& path, HMODULE hMod, int32 rcid)
{
    bool bResult = false;

    HOBJ hFile = FileOpen(RcFsBuildPath(hMod, rcid), "rb");
    if (hFile)
    {
        uint32 fsize = uint32(FileSize(hFile));

        std::vector<char> buffer;
        buffer.resize(std::size_t(fsize));

        uint32 bytes = FileRead(hFile, &buffer[0], buffer.size());
        ASSERT(bytes == fsize);

        bResult = load(path, &buffer[0], buffer.size());

        FileClose(hFile);
    };

    return bResult;
};


void nk_text_container::unload(const std::string& path)
{
    auto it = std::find_if(m_textList.begin(), m_textList.end(), [&](std::shared_ptr<struct nk_text>& ptr) {
        return (ptr->path == path);
    });

    if (it != m_textList.end())
        m_textList.erase(it);
};


const char* nk_text_container::get_text(std::size_t id) const
{
    ASSERT(m_textStack.empty() == false);
    ASSERT(id < m_textStack.top()->offsets.size());

    if (id < m_textStack.top()->offsets.size())
        return &m_textStack.top()->data[m_textStack.top()->offsets[id]];
    else
        return "TEXT_ERROR";
};


static std::shared_ptr<nk_text_container> s_pNkTextContainer = nullptr;


void nk_text_init()
{
    s_pNkTextContainer = std::make_shared<nk_text_container>();
    nk_text_set_locale(nk_text_locale_sysdefault);
};


void nk_text_term()
{
    s_pNkTextContainer = nullptr;
};


void nk_text_set_locale(nk_text_locale locale)
{
    s_pNkTextContainer->set_locale(locale);
};


/*DLLSHARED*/ bool nk_text_load(const char* path, const void* data, std::size_t size)
{
    return s_pNkTextContainer->load(path, data, size);
};


/*DLLSHARED*/ bool nk_text_load_rc(const char* path, int32 rcid)
{
    return nk_text_load_mod_rc(path, GetModuleHandle(NULL), rcid);
};


/*DLLSHARED*/ bool nk_text_load_mod_rc(const char* path, HMODULE hMod, int32 rcid)
{
    return s_pNkTextContainer->load_rc(path, hMod, rcid);
};


/*DLLSHARED*/ void nk_text_unload(const char* path)
{
    s_pNkTextContainer->unload(path);
};


/*DLLSHARED*/ bool nk_text_path_push(const char* path)
{
    return s_pNkTextContainer->path_push(path, false);
};


/*DLLSHARED*/ bool nk_text_path_locale_push(const char* path)
{
    return s_pNkTextContainer->path_push(path, true);
};


/*DLLSHARED*/ void nk_text_path_pop()
{
    s_pNkTextContainer->path_pop();
};


/*DLLSHARED*/ const char* nk_text_id(std::size_t id)
{
    return s_pNkTextContainer->get_text(id - 1u);
};

