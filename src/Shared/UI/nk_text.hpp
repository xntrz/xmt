#pragma once


enum nk_text_locale
{
    nk_text_locale_en = 0,
    nk_text_locale_ru,

    nk_text_locale_default = nk_text_locale_en,
    nk_text_locale_sysdefault = -1,    
};


void nk_text_init();
void nk_text_term();
void nk_text_set_locale(nk_text_locale locale);

DLLSHARED bool nk_text_load(const char* path, const void* data, std::size_t size);
DLLSHARED bool nk_text_load_rc(const char* path, int32 rcid);
DLLSHARED bool nk_text_load_mod_rc(const char* path, HMODULE hMod, int32 rcid);
DLLSHARED void nk_text_unload(const char* path);
DLLSHARED bool nk_text_path_push(const char* path);
DLLSHARED bool nk_text_path_locale_push(const char* path);
DLLSHARED void nk_text_path_pop();
DLLSHARED const char* nk_text_id(std::size_t id);