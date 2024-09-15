#include "nk_utils.hpp"
#include "nk_window.hpp"

#include "Shared/Common/Time.hpp"


/*DLLSHARED*/ std::wstring nku_utf8_to_wstr(const std::string& str)
{
    int wsize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0);
    std::wstring wstr;
    wstr.resize(wsize);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), &wstr[0], wsize);
    return wstr;
};


/*DLLSHARED*/ void nku_adjust_window_size(struct nkgdi_window* wnd, struct nk_context* ctx)
{
    int32 ww = 0;
    int32 wh = 0;
    nkgdi_window_get_size(wnd, &ww, &wh);

    struct nk_rect bounds = nk_widget_bounds(ctx);
    bounds.y += (bounds.h * 2.0f);

    nkgdi_window_set_min_size(wnd, int32(bounds.w), int32(bounds.y));

    if (ww < int32(bounds.x + bounds.w))
        ww = int32(bounds.x + bounds.w);
    if (wh < int32(bounds.y))
        wh = int32(bounds.y);

    nkgdi_window_set_size(wnd, ww, wh);
};


/*DLLSHARED*/ void nku_get_row_pos(struct nkgdi_window* wnd, struct nk_context* ctx, float* x, float* y)
{
    ASSERT(wnd);
    ASSERT(ctx);
    ASSERT(x);
    ASSERT(y);
    ASSERT(ctx->current);
    ASSERT(ctx->current->layout);

    *x = ctx->current->layout->clip.w;
    *y = ctx->current->layout->clip.h;
};


/*DLLSHARED*/ float nku_get_row_pos_x(struct nkgdi_window* wnd, struct nk_context* ctx)
{
    float x = 0.0f;
    float y = 0.0f;
    nku_get_row_pos(wnd, ctx, &x, &y);
    return x;
};


/*DLLSHARED*/ float nku_get_row_pos_y(struct nkgdi_window* wnd, struct nk_context* ctx)
{
    float x = 0.0f;
    float y = 0.0f;
    nku_get_row_pos(wnd, ctx, &x, &y);
    return y;
};


/*DLLSHARED*/ void nku_label_time_format(struct nk_context* ctx, uint32 time, nk_flags align)
{
    uint32 h = 0;
    uint32 m = 0;
    uint32 s = 0;
    uint32 ms = 0;
    TimeStampSlice(time, &h, &m, &s, &ms);

    char value[256];
    std::sprintf(value, "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32, h, m, s);

    nk_label(ctx, value, align);
};