#pragma once

#include "nk_includes.hpp"


//typedef bool (*cb_draw)(struct nkgdi_window* wnd, struct nk_context* ctx, bool ret);
using cb_draw = std::function<bool(struct nkgdi_window* wnd, struct nk_context* ctx, bool ret)>;


struct nkgdi_window;


enum nkgdi_window_theme
{
    nkgdi_window_theme_dark = 0,
    
    nkgdi_window_theme_sys = -1,
};


enum nkgdi_window_flag
{
    nkgdi_window_flag_min       = (1 << 0),
    nkgdi_window_flag_max       = (1 << 1),
    nkgdi_window_flag_close     = (1 << 2),
    nkgdi_window_flag_resize    = (1 << 3),
    nkgdi_window_flag_layered   = (1 << 4),
    nkgdi_window_flag_ico       = (1 << 5),
    nkgdi_window_flag_disable_return = (1 << 6),
	nkgdi_window_flag_hide_return = (1 << 7),
};


enum nkgdi_window_msg
{
    nkgdi_window_msg_fdrop = 0,
    nkgdi_window_msg_extend = 100,
};


#define nkgdi_window_obj_drawproc(obj, fn) \
    (std::bind(fn, obj, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))


void nkgdi_window_init(nkgdi_window_theme theme);
void nkgdi_window_shutdown();
void nkgdi_window_loop();
void nkgdi_window_locale_changed();
DLLSHARED bool nkgdi_window_show(const char* title, int32 w, int32 h, int32 x, int32 y, const char* fontName, int32 fontHeight, uint32 flags, cb_draw cb, struct nkgdi_window* parent, void* param);
DLLSHARED void nkgdi_window_push_draw_proc(struct nkgdi_window* wnd, cb_draw cb);
DLLSHARED void nkgdi_window_set_draw_proc(struct nkgdi_window* wnd, cb_draw cb);
DLLSHARED uint32 nkgdi_window_get_flags(struct nkgdi_window* wnd);
DLLSHARED void nkgdi_window_set_flags(struct nkgdi_window* wnd, uint32 flags);
DLLSHARED void nkgdi_window_post_redraw(struct nkgdi_window* wnd);
DLLSHARED void nkgdi_window_set_title(struct nkgdi_window* wnd, const char* title);
DLLSHARED void nkgdi_window_get_title(struct nkgdi_window* wnd, const char* title, int32* size);
DLLSHARED void nkgdi_window_set_size(struct nkgdi_window* wnd, int32 w, int32 h);
DLLSHARED void nkgdi_window_get_size(struct nkgdi_window* wnd, int32* w, int32* h);
DLLSHARED void nkgdi_window_set_min_size(struct nkgdi_window* wnd, int32 w, int32 h);
DLLSHARED void nkgdi_window_send_msg(struct nkgdi_window* wnd, int32 id, intptr_t param);
DLLSHARED bool nkgdi_window_recv_msg(struct nkgdi_window* wnd, int32* id, intptr_t* param);
DLLSHARED void nkgdi_window_set_param(struct nkgdi_window* wnd, void* param);
DLLSHARED void nkgdi_window_get_param(struct nkgdi_window* wnd, void** param);
DLLSHARED HWND nkgdi_window_get_native(struct nkgdi_window* wnd);
DLLSHARED void nkgdi_window_set_ico(struct nkgdi_window* wnd, struct nk_image* img);
DLLSHARED bool nk_img_create(struct nk_image* img, const char* path);
DLLSHARED bool nk_img_create_rc(struct nk_image* img, int32 rcid);
DLLSHARED bool nk_img_create_mod_rc(struct nk_image* img, HINSTANCE hMod, int32 rcid);
DLLSHARED bool nk_img_create_mem(struct nk_image* img, const void* buff, uint32 size);
DLLSHARED void nk_img_destroy(struct nk_image* img);