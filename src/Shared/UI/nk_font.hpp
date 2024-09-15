#pragma once

bool nk_font_install(HINSTANCE hInstance, int32 rcid);
void nk_font_uninstall();
DLLSHARED void nk_font_set_height(int32 h);
DLLSHARED const char* nk_font_get_name();
DLLSHARED int32 nk_font_get_height();