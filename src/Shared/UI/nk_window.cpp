#include "nk_window.hpp"
#include "nk_utils.hpp"

#include "Shared/Common/Configuration.hpp"
#include "../File/File.hpp"
#include "../File/FsRc.hpp"

/**
 *  GDI+ INCLUDES
 */
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#else
#error max macro is already defined
#endif
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#else
#error min macro is already defined
#endif

#pragma push_macro("new")
#pragma push_macro("delete")
#undef new
#undef delete
#include <objidl.h>
#include <gdiplus.h>
#include <Shlwapi.h>
#pragma pop_macro("delete")
#pragma pop_macro("new")

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

/**
 *  NUKLEAR INCLUDES
 */
#define NK_IMPLEMENTATION
#define NK_GDI_IMPLEMENTATION
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_BUTTON_TRIGGER_ON_RELEASE
#include "nuklear.h"
#include "nuklear_gdi.h"


#define NK_GDI_WINDOW_CLS L"WNDCLS_NkGdi"


enum nk_sys_button_icon
{
    nk_sys_button_icon_return = 0,
    nk_sys_button_icon_home,
    nk_sys_button_icon_min,
    nk_sys_button_icon_max,
    nk_sys_button_icon_restore,
    nk_sys_button_icon_close,
};


enum nk_sys_button_state
{
    nk_sys_button_state_return  = (1 << 0),
    nk_sys_button_state_min     = (1 << 1),
    nk_sys_button_state_max     = (1 << 2),
    nk_sys_button_state_restore = (1 << 3),
    nk_sys_button_state_close   = (1 << 4),
};


static std::list<std::shared_ptr<nkgdi_window>> s_wndList;
static std::atomic<std::size_t>     s_imgRefCnt = 0;
static nkgdi_window_theme           s_theme = nkgdi_window_theme_dark;
static ULONG_PTR                    s_gdipToken = NULL;
static bool                         s_bIsProcessClassOwner = false;
static uint32                       s_wndUidCounter = 0;


struct nkgdi_window
{
    struct message
    {
        int32 id;
        intptr_t param;
    };

    std::vector<cb_draw> drawStack;
    std::string title;
    std::deque<message> messages;
    std::array<void*, 8> params;
    HDC window_dc;
    HWND window_handle;
    nk_gdi_ctx nk_gdi_ctx;
    struct nk_context* nk_ctx;
    GdiFont* gdi_font;
    union {
        struct {
            uint32 is_open : 1;
            uint32 is_return : 1;
            uint32 is_maximized : 1;
            uint32 is_dragging : 1;
            uint32 ws_override : 1;
            uint32 is_hover_title : 1;
			uint32 is_drawing : 1;
            uint32 set_size : 1;
        };
        uint16 flagsPrivate;
    };
    uint16 flags;
    struct nk_vec2i drag_offset;
    int32 width;
    int32 height;
    int32 width_real;
    int32 height_real;
    struct nk_image* ico_img;
    struct nk_rect boundPrev;
    char label[32];

    inline nkgdi_window()
    : drawStack()
    , title()
    , messages()
    , params()
    , window_dc(NULL)
    , window_handle(NULL)
    , nk_gdi_ctx(0)
    , nk_ctx(nullptr)
    , gdi_font(nullptr)
    , flagsPrivate(0)
    , flags(0)
    , drag_offset()
    , width(0)
    , height(0)
    , width_real(0)
    , height_real(0)
    , ico_img(nullptr)
    , boundPrev({ 0, 0, 0, 0 })
    , label()
    {
        drag_offset.x = 0;
		drag_offset.y = 0;

        label[0] = '\0';
        std::sprintf(label, "%s_%" PRIu32, "wnd", s_wndUidCounter++);
    };

    ~nkgdi_window() { 

        if (nk_gdi_ctx)
            nk_gdi_shutdown(nk_gdi_ctx);

        if (gdi_font)
            nk_gdifont_del(gdi_font);

        if (window_dc)
            ReleaseDC(window_handle, window_dc);
        //nk_gdi_shutdown(nk_gdi_ctx); 
    };
};


static inline void nk_size_correction(int32* w, int32* h)
{
    ASSERT(w && h);

    if (*w < 200)
        *w = 200;
    
    if (*h < 200)
        *h = 200;
};


static inline nk_color nk_disable_color(nk_color col)
{
    uint8 col_x = uint8(
        (float(col.r) * 0.01f) +
        (float(col.g) * 0.25f) +
        (float(col.b) * 0.1f)
    );
    return nk_rgb(col_x, col_x, col_x);
};


static void nkgdi_window_center(struct nkgdi_window* wnd)
{
	HMONITOR hMon = MonitorFromWindow(wnd->window_handle, MONITOR_DEFAULTTONEAREST);
	if (hMon)
	{
		MONITORINFO moninfo;
		std::memset(&moninfo, 0, sizeof(moninfo));
		moninfo.cbSize = sizeof(moninfo);

		if (GetMonitorInfo(hMon, &moninfo))
		{
			RECT rc;
			SetRect(&rc, 0, 0, 0, 0);
			GetWindowRect(wnd->window_handle, &rc);

			int w = rc.right - rc.left;
			int h = rc.bottom - rc.top;

			int x = (moninfo.rcWork.left + moninfo.rcWork.right) / 2 - w / 2;
			int y = (moninfo.rcWork.top + moninfo.rcWork.bottom) / 2 - h / 2;

			SetWindowPos(wnd->window_handle, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOOWNERZORDER);
		};
	};
};


static nkgdi_window_theme nk_detect_theme()
{
    return nkgdi_window_theme_dark;
};


static void nk_apply_theme(struct nk_context* ctx)
{
    struct nk_color table[NK_COLOR_COUNT];

    switch (s_theme)
    {
    case nkgdi_window_theme_dark:
        table[NK_COLOR_TEXT] = nk_rgba(255, 255, 255, 255);
        table[NK_COLOR_WINDOW] = nk_rgba(0, 0, 0, 255);
        table[NK_COLOR_HEADER] = nk_rgba(175, 175, 175, 255);
        table[NK_COLOR_BORDER] = nk_rgba(173, 173, 173, 255);
        table[NK_COLOR_BUTTON] = nk_rgba(51, 51, 51, 255);
        table[NK_COLOR_BUTTON_HOVER] = nk_rgba(51, 51, 51, 255);
        table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(102, 102, 102, 255);
        table[NK_COLOR_TOGGLE] = nk_rgba(102, 102, 102, 255);
        table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(255, 255, 255, 255);
        table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(0, 120, 215, 255);
        //table[NK_COLOR_SELECT] = nk_rgba(0, 190, 0, 255);
        //table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(225, 225, 225, 255);
        table[NK_COLOR_SLIDER] = nk_rgba(102, 102, 102, 255);
        table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(0, 120, 215, 255);
        table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(255, 255, 255, 255);
        table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(0, 120, 215, 255);
        table[NK_COLOR_PROPERTY] = nk_rgba(102, 102, 102, 255);
        table[NK_COLOR_EDIT] = nk_rgba(0, 0, 0, 255);
        //table[NK_COLOR_EDIT_CURSOR] = nk_rgba(0, 0, 0, 255);
        table[NK_COLOR_COMBO] = nk_rgba(0, 0, 0, 255);
        //table[NK_COLOR_CHART] = nk_rgba(160, 160, 160, 255);
        //table[NK_COLOR_CHART_COLOR] = nk_rgba(45, 45, 45, 255);
        //table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(255, 0, 0, 255);
        table[NK_COLOR_SCROLLBAR] = nk_rgba(27, 27, 27, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(73, 73, 73, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(118, 118, 118, 255);
        table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(164, 164, 164, 255);
        table[NK_COLOR_TAB_HEADER] = nk_rgba(0, 0, 0, 255);
        break;

    default:
        ASSERT(false);
        break;
    };
    
    nk_style_from_table(ctx, table);

    /* scroll bar */
    ctx->style.scrollv.dec_symbol = NK_SYMBOL_TRIANGLE_DOWN;
    ctx->style.scrollv.inc_symbol = NK_SYMBOL_TRIANGLE_UP;

    /* buttons */
    ctx->style.button.border = 0.0f;
    ctx->style.button.rounding = 0.0f;
    //ctx->style.button.text_background = { 0 };

    /* edit */
    ctx->style.edit.padding = nk_vec2(0, 0);
    ctx->style.edit.row_padding = 0.0f;
    ctx->style.edit.scrollbar_size = nk_vec2(0, 0);
    ctx->style.edit.cursor_size = 1.0f;

    /* opt */
    ctx->style.option.padding = nk_vec2(0.0f, 0.0f);

    /* progress bar */
    ctx->style.progress.padding = nk_vec2(0.0f, 0.0f);

    /* window */
    ctx->style.window.min_size = nk_vec2(200.0f, 200.0f);
    ctx->style.window.border = 1.0f;
	ctx->style.window.border_color = nk_rgb(39, 39, 39);
    ctx->style.window.padding = nk_vec2(0.0f, 0.0f);
    ctx->style.window.scrollbar_size = nk_vec2(16.0f, 16.0f);
    ctx->style.window.header.normal.data.color = nk_rgb(25, 25, 25);
    ctx->style.window.header.hover.data.color = nk_rgb(25, 25, 25);
    ctx->style.window.header.active.data.color = nk_rgb(25, 25, 25);
    ctx->style.window.header.padding = nk_vec2(2.0f, 2.0f);
    ctx->style.window.header.label_padding = nk_vec2(2.0f, 2.0f);

    /* sel */
    ctx->style.selectable.normal.data.color = nk_rgb(0, 255, 0);
};


static bool nk_draw_sys_button_icon(
    struct nk_context*          ctx,
    struct nk_rect              bounds,
    struct nk_command_buffer*   out,
    enum nk_sys_button_icon     icon,
    struct nk_style_button*     style,
    bool                        bDisabled
)
{
    float factor = (bDisabled ? style->disabled_factor : 1.0f);

    switch (icon)
    {
    case nk_sys_button_icon_home:
        {
            float ofs_x = 0.5f;
            float ofs_y = 0.5f;
            float w = 18.0f;
            float h = 14.0f;

            struct nk_rect r = nk_rect(
                bounds.x + (bounds.w * ofs_x) - (w * 0.5f),
                bounds.y + (bounds.h * ofs_y) - (h * 0.5f),
                w,
                h
            );

            float points[] =
            {
                r.x + (r.w * 0.5f) - 1.0f,                  r.y,
                r.x + 1.0f,                                 r.y + (r.h * 0.5f),
                r.x + 3.0f,                                 r.y + (r.h * 0.5f) - 1.0f,
                r.x + 3.0f,                                 r.y + (r.h * 0.5f) + 7.0f,
                r.x + 3.0f + 4.0f,                          r.y + (r.h * 0.5f) + 7.0f,
                r.x + 3.0f + 4.0f,                          r.y + (r.h * 0.5f) + 1.0f,
                r.x + 3.0f + 4.0f + 2.0f,                   r.y + (r.h * 0.5f) + 1.0f,
                r.x + 3.0f + 4.0f + 2.0f,                   r.y + (r.h * 0.5f) + 7.0f,
                r.x + 3.0f + 4.0f + 2.0f + 4.0f,            r.y + (r.h * 0.5f) + 7.0f,
                r.x + 3.0f + 4.0f + 2.0f + 4.0f,            r.y + (r.h * 0.5f) - 1.0f,
                r.x + 3.0f + 4.0f + 2.0f + 4.0f + 2.0f,     r.y + (r.h * 0.5f),
            };

            nk_color c1 = nk_rgb_factor(nk_rgb(255, 255, 255), factor);

            nk_stroke_polygon(out, points, (sizeof(points) / sizeof(points[0])) / 2, 1.0f, c1);
        }
        break;

    case nk_sys_button_icon_return:
        {
            float ofs_x = 0.5f;
            float ofs_y = 0.5f;

            struct nk_rect r = nk_rect(
                bounds.x + (bounds.w * ofs_x) - (12.0f * 0.5f),
                bounds.y + (bounds.h * ofs_y),
                12.0f,
                12.0f
            );

            nk_color c1 = nk_rgb_factor(nk_rgb(255, 255, 255), factor);
            nk_color c2 = nk_rgb_factor(nk_rgb(240, 240, 240), factor);

            nk_stroke_line(out, r.x, r.y, r.x + r.w, r.y, 1.0f, c1);
            nk_stroke_line(out, r.x, r.y, r.x + (r.w * 0.5f), r.y + (r.w * 0.5f), 1.5f, c2);
            nk_stroke_line(out, r.x, r.y, r.x + (r.h * 0.5f), r.y - (r.h * 0.5f), 1.5f, c2);
        }
        break;

    case nk_sys_button_icon_min:
        {
            float ofs_x = 0.5f;
            float ofs_y = 0.5f;
            float w = 10.0f;
            float h = 1.0f;

            struct nk_rect r = nk_rect(
                bounds.x + (bounds.w * ofs_x) - (w * 0.5f),
                bounds.y + (bounds.h * ofs_y) - (h * 0.5f),
                w,
                h
            );

            nk_color c1 = nk_rgb_factor(nk_rgb(255, 255, 255), factor);

            nk_stroke_rect(out, r, 0.0f, 1.0f, c1);
        }
        break;

    case nk_sys_button_icon_max:
        {
            float ofs_x = 0.5f;
            float ofs_y = 0.5f;
            float w = 10.0f;
            float h = 10.0f;

            struct nk_rect r = nk_rect(
                bounds.x + (bounds.w * ofs_x) - (w * 0.5f),
                bounds.y + (bounds.h * ofs_y) - (h * 0.5f),
                w,
                h
            );

            nk_color c1 = nk_rgb_factor(nk_rgb(255, 255, 255), factor);

            nk_stroke_rect(out, r, 0.0f, 1.0f, c1);
        }
        break;

    case nk_sys_button_icon_restore:
        {
            float ofs_x = 0.5f;
            float ofs_y = 0.5f;
            float w = 8.0f;
            float h = 8.0f;

            struct nk_rect r = nk_rect(
                bounds.x + (bounds.w * ofs_x) - (w * 0.5f),
                bounds.y + (bounds.h * ofs_y) - (h * 0.5f),
                w,
                h
            );

            nk_color c1 = nk_rgb_factor(nk_rgb(255, 255, 255), factor);

            r.x += 1.0f;
            r.y -= 1.0f;
            nk_stroke_rect(out, r, 0.0f, 1.0f, c1);

            r.x -= 2.0f;
            r.y += 2.0f;
            nk_fill_rect(out, r, 0.0f, nk_rgb(0, 0, 0));
            nk_stroke_rect(out, r, 0.0f, 1.0f, c1);
        }
        break;

    case nk_sys_button_icon_close:
        {
            float ofs_x = 0.5f;
            float ofs_y = 0.5f;
            float w = 10.0f;
            float h = 10.0f;

            struct nk_rect r = nk_rect(
                bounds.x + (bounds.w * ofs_x) - (w * 0.5f),
                bounds.y + (bounds.h * ofs_y) - (h * 0.5f),
                w,
                h
            );

            nk_color c1 = nk_rgb_factor(nk_rgb(255, 255, 255), factor);

            nk_stroke_line(
                out,
                r.x,
                r.y + 1.0f,
                r.x + w,
                r.y + h + 1.0f,
                1.0f,
                c1
            );

            nk_stroke_line(
                out,
                r.x,
                r.y + h,
                r.x + w,
                r.y,
                1.0f,
                c1
            );
        }
        break;

    default:
        ASSERT(false);
        break;
    };

    return true;
};


static bool nk_do_button_common_styled(struct nk_context* ctx, struct nk_rect bounds, struct nk_command_buffer* out, struct nk_style_button* style, enum nk_widget_layout_states state)
{
    const struct nk_input* in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED) ? nullptr : &ctx->input;

    struct nk_rect content;
	bool bResult = (nk_do_button(&ctx->last_widget_state, out, bounds, style, in, ctx->button_behavior, &content) > 0);

    nk_draw_button(out, &bounds, ctx->last_widget_state, style);

    return bResult;
};


static bool nk_sys_button_styled(
    nkgdi_window*           wnd,
    struct nk_context*      ctx,
    struct nk_style_button* style,
    enum nk_sys_button_icon icon
)
{
    bool bResult = false;
    struct nk_command_buffer* out = nk_window_get_canvas(ctx);
    struct nk_rect bounds;
    enum nk_widget_layout_states state = nk_widget(&bounds, ctx);
    if (!state)
        return bResult;
    
    bool bDisabled = (state == NK_WIDGET_DISABLED);

    bResult = nk_do_button_common_styled(ctx, bounds, out, style, state);
    nk_draw_sys_button_icon(ctx, bounds, out, icon, style, bDisabled);
    return bResult;
};


static uint32 nk_sys_bar(nkgdi_window* wnd, struct nk_context* ctx)
{
    uint32 state = 0;
	uint32 flags = wnd->flags;

	if ((flags & nkgdi_window_flag_ico) && (!wnd->ico_img))
		flags &= ~nkgdi_window_flag_ico;

    /**
     *  setup sys bar layout template
     *  5 elements max
     */
    nk_layout_row_template_begin(ctx, 30);
	if ((flags & nkgdi_window_flag_layered) && !(flags & nkgdi_window_flag_hide_return))
        nk_layout_row_template_push_static(ctx, 48);
    if (flags & nkgdi_window_flag_ico)
        nk_layout_row_template_push_static(ctx, 32);
    nk_layout_row_template_push_dynamic(ctx);
    if (flags & nkgdi_window_flag_min)
        nk_layout_row_template_push_static(ctx, 46);
    if (flags & nkgdi_window_flag_max)
        nk_layout_row_template_push_static(ctx, 46);
    if (flags & nkgdi_window_flag_close)
        nk_layout_row_template_push_static(ctx, 46);
    nk_layout_row_template_end(ctx);

    /**
     *  draw return or home button
     */
    if ((flags & nkgdi_window_flag_layered) && !(flags & nkgdi_window_flag_hide_return))
    {
        if (wnd->drawStack.size() > 1)
        {
            if (wnd->flags & nkgdi_window_flag_disable_return)
                nk_widget_disable_begin(ctx);
            
            nk_style_push_color(ctx, &ctx->style.button.hover.data.color, nk_rgb(25, 133, 218));
            nk_style_push_color(ctx, &ctx->style.button.active.data.color, nk_rgb(51, 147, 223));
            if (nk_sys_button_styled(wnd, ctx, &ctx->style.button, nk_sys_button_icon_return))
                state |= nk_sys_button_state_return;
            nk_style_pop_color(ctx);
            nk_style_pop_color(ctx);

            if (wnd->flags & nkgdi_window_flag_disable_return)
                nk_widget_disable_end(ctx);
        }
        else
        {
            nk_sys_button_styled(wnd, ctx, &ctx->style.button, nk_sys_button_icon_home);
        };
    };

    /**
     *  ico image
     */
	if (flags & nkgdi_window_flag_ico)
	{
        if (wnd->ico_img)
        {
            struct nk_command_buffer* out = nk_window_get_canvas(ctx);
            
			struct nk_rect bounds;
            nk_widget(&bounds, ctx);

            bounds.w *= 0.75f;
            bounds.h *= 0.75f;
            bounds.x += bounds.w * (0.25f * (bounds.h / bounds.w));
			bounds.y += bounds.h * (0.25f * (bounds.h / bounds.w));
            nk_draw_image(out, bounds, wnd->ico_img, nk_rgb(255, 255, 255));
        };
	};
    
    /**
     *  draw title bar
     */
	nk_style_push_vec2(
		ctx, 
		&ctx->style.text.padding, 
		nk_vec2(flags & nkgdi_window_flag_ico ? 2.0f : 13.0f, 0.0f)
	);
    
	{
		struct nk_rect bounds = nk_widget_bounds(ctx);
		nk_label(ctx, wnd->title.c_str(), NK_TEXT_LEFT);
		if (nk_input_is_mouse_hovering_rect(&ctx->input, bounds))
			wnd->is_hover_title = true;
		else
			wnd->is_hover_title = false;
    }

	nk_style_pop_vec2(ctx);

    /**
     *  draw minimize button
     */
    if (flags & nkgdi_window_flag_min)
    {
        if (nk_sys_button_styled(wnd, ctx, &ctx->style.button, nk_sys_button_icon_min))
            state |= nk_sys_button_state_min;
    };

    /**
     *  draw maximize or restore button
     */
    if (flags & nkgdi_window_flag_max)
    {
        if (!wnd->is_maximized)
        {
            if (nk_sys_button_styled(wnd, ctx, &ctx->style.button, nk_sys_button_icon_max))
                state |= nk_sys_button_state_max;
        }
        else
        {
            if (nk_sys_button_styled(wnd, ctx, &ctx->style.button, nk_sys_button_icon_restore))
                state |= nk_sys_button_state_restore;
        };
    };

    /**
     *  draw close button
     */
    if (flags & nkgdi_window_flag_close)
    {
        nk_style_push_color(ctx, &ctx->style.button.hover.data.color, nk_rgb(232, 17, 35));
        nk_style_push_color(ctx, &ctx->style.button.active.data.color, nk_rgb(241, 112, 122));
        if (nk_sys_button_styled(wnd, ctx, &ctx->style.button, nk_sys_button_icon_close))
            state |= nk_sys_button_state_close;
        nk_style_pop_color(ctx);
        nk_style_pop_color(ctx);
    };

    return state;
};


static void nkgdi_window_update_sys()
{
	/* update system messages (this mostly for correct chaning keyboard layout - see WM_INPUTLANGCHANGE) */
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE | PM_QS_SENDMESSAGE | PM_QS_POSTMESSAGE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	};
};


static bool nkgdi_window_update(nkgdi_window* wnd)
{
    ASSERT(wnd);
    
    struct nk_context* ctx = wnd->nk_ctx;

    int32 msgCount = 0;
	MSG msg;
	nk_input_begin(ctx);
	while (PeekMessage(&msg, wnd->window_handle, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
        ++msgCount;
	};
	nk_input_end(ctx);

	if (wnd->is_open && !wnd->is_drawing && (msgCount > 0))
	{
		wnd->is_drawing = 1;
        nk_flags wndflags = NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER;
        wndflags |= (wnd->flags & nkgdi_window_flag_resize ? NK_WINDOW_SCALABLE : 0);

		/* maximized window cannot be scaled */
        if (wnd->is_maximized)
            wndflags &= ~NK_WINDOW_SCALABLE;

        /* check for resize and cancel ws override if is it true */
        nk_window* w = nk_window_find(ctx, wnd->label);
        if (w)
        {
            struct nk_rect boundNow = w->bounds;
            if ((boundNow.w != wnd->boundPrev.w) ||
                (boundNow.h != wnd->boundPrev.h))
            {
                wnd->ws_override = 0;
                SetWindowPos(wnd->window_handle, NULL, 0, 0, int(boundNow.w), int(boundNow.h), SWP_DEFERERASE | SWP_NOMOVE | SWP_NOOWNERZORDER);
            };
        };

        /* window size change request handling */
        bool ws_override = false;
        if (wnd->ws_override)
        {
            nk_window_set_bounds(wnd->nk_ctx, wnd->label, nk_rect(0, 0, float(wnd->width), float(wnd->height)));
            wnd->ws_override = 0;
            ws_override = true;
        };

        if (nk_begin(ctx, wnd->label, nk_rect(0, 0, float(wnd->width), float(wnd->height)), wndflags))
        {
            wnd->width_real = wnd->width;
            wnd->height_real = wnd->height;

            /* apply window theme */
            nk_apply_theme(ctx);

            /* draw sys bar */
            nk_style_push_float(ctx, &ctx->style.button.border, 0.0f);
            nk_style_push_color(ctx, &ctx->style.button.normal.data.color, nk_rgb(0, 0, 0));
            nk_style_push_color(ctx, &ctx->style.button.hover.data.color, nk_rgb(25, 25, 25));
            nk_style_push_color(ctx, &ctx->style.button.active.data.color, nk_rgb(53, 53, 53));
            uint32 state = nk_sys_bar(wnd, ctx);
            nk_style_pop_color(ctx);
            nk_style_pop_color(ctx);
            nk_style_pop_color(ctx);
            nk_style_pop_float(ctx);

            /* handle sys bar state */
            if (state & nk_sys_button_state_close)
            {
                wnd->is_open = false;
            };
            if (state & nk_sys_button_state_return)
            {
                if (!wnd->drawStack.empty())
                {
                    wnd->drawStack.pop_back();
                    wnd->is_return = 1;
                };
            };
            if (state & nk_sys_button_state_min)
            {
                ShowWindowAsync(wnd->window_handle, SW_MINIMIZE);
            };
            if (state & nk_sys_button_state_max)
            {
                ShowWindowAsync(wnd->window_handle, SW_MAXIMIZE);
                wnd->ws_override = 1;
            };
            if (state & nk_sys_button_state_restore)
            {
                ShowWindowAsync(wnd->window_handle, SW_RESTORE);
                wnd->ws_override = 1;
            };

            /* call user draw proc */
            if (!wnd->drawStack.empty())
            {
                wnd->is_return = wnd->drawStack.back()(wnd, ctx, bool(wnd->is_return));
                if (wnd->is_return)
                {
                    wnd->drawStack.pop_back();
                    nkgdi_window_post_redraw(wnd);
                };
            }
            else
            {
                wnd->is_open = 0;
            };

            /* fit win32 window size to nuklear window size */
            struct nk_rect bounds = nk_window_get_bounds(ctx);
            if ((int(bounds.w) != wnd->width) || (int(bounds.h) != wnd->height) || ws_override)
                SetWindowPos(wnd->window_handle, NULL, 0, 0, int(bounds.w), int(bounds.h), SWP_DEFERERASE | SWP_NOMOVE | SWP_NOOWNERZORDER);
        };
        wnd->boundPrev = nk_window_get_bounds(ctx);
        nk_end(wnd->nk_ctx);

		/* flush draw commands */
		nk_gdi_render(wnd->nk_gdi_ctx, nk_rgb(0, 255, 0));
		
        if (!wnd->is_open)
        {
            CloseWindow(wnd->window_handle);
			DestroyWindow(wnd->window_handle);
		};

		wnd->is_drawing = 0;
	};

	return wnd->is_open;
};


static void nkgdi_window_loop_common(nkgdi_window* wnd = nullptr)
{
    /* break flag for check close of modal dialog */
    bool bBreak = false;
    
    while (true)
    {
        /* wait for any thread messages */
        WaitMessage();

        /* process window specific messages */
        auto it = s_wndList.begin();
        auto itEnd = s_wndList.end();
        while (it != itEnd)
        {
            if (!nkgdi_window_update(it->get()))
            {
                bBreak = (it->get() == wnd);
                it = s_wndList.erase(it);
            }
            else
            {
                ++it;
            };
        };

        /**
         *  process system messages
         *  NOTE: without this cannot handle keyboard layout change (WM_INPUTLANGCHANGE sends not to main window!!)
         */
        nkgdi_window_update_sys();

        /* check for stop */
        if (s_wndList.empty() || bBreak)
            break;
    };
};


static LRESULT CALLBACK nkgdi_window_proc_run(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    struct nkgdi_window* nkwnd = (struct nkgdi_window*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    
    switch (msg)
    {
	case WM_DESTROY:
        {
            if (nk_begin(nkwnd->nk_ctx, "", nk_rect(0, 0, float(nkwnd->width_real), float(nkwnd->height_real)), 0))
            {
                while (!nkwnd->drawStack.empty())
                {
                    std::invoke(nkwnd->drawStack.back(), nkwnd, nkwnd->nk_ctx, true);
                    nkwnd->drawStack.pop_back();
                };
            };
            nk_end(nkwnd->nk_ctx);
        }        
        break;

    case WM_SIZING:
        {
			RECT cr;
            GetClientRect(hwnd, &cr);
            nkwnd->width = cr.right - cr.left;
            nkwnd->height = cr.bottom - cr.top;
        }
        break;
        
    case WM_SIZE:
        {
            if (wParam == SIZE_MAXIMIZED)
            {
                HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
                MONITORINFO monitorInfo;
                monitorInfo.cbSize = sizeof(MONITORINFO);
                if (GetMonitorInfoW(monitor, &monitorInfo))
                {
                    nkwnd->height = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
                    nkwnd->width = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
                    nkwnd->is_maximized = 1;
                    nkwnd->ws_override = 1;
                    SetWindowPos(hwnd, NULL, 0, 0, nkwnd->width, nkwnd->height, SWP_NOMOVE | SWP_NOZORDER);
                };
            }
            else if (wParam == SIZE_RESTORED)
            {
                nkwnd->is_maximized = 0;
            }
            else if (wParam == SIZE_MINIMIZED)
            {
                /**
                 *  mouse buttons are still in down state after restore
                 *  so reset state manually before window minimize
                 */
                nk_input_button(nkwnd->nk_ctx, NK_BUTTON_LEFT, 0, 0, 0);
                nk_input_button(nkwnd->nk_ctx, NK_BUTTON_RIGHT, 0, 0, 0);
            };

            RECT cr;
            GetClientRect(hwnd, &cr);
            nkwnd->width = cr.right - cr.left;
            nkwnd->height = cr.bottom - cr.top;            
			InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

    case WM_LBUTTONDBLCLK:
        {
            if (nkwnd->is_hover_title)
            {
                /* When the window is already maximized restore it */
                if (nkwnd->is_maximized)
                {
                    ShowWindow(hwnd, SW_RESTORE);
                }
                /* Else we gonna do maximize it*/
                else
                {
                    ShowWindow(hwnd, SW_MAXIMIZE);
                }
                /* We overrideed the window size, make sure to affect the nk window as well */
                nkwnd->ws_override = 1;
            };            
        }
        break;

    case WM_LBUTTONDOWN:
        {
            if (nkwnd->is_hover_title) 
			{
                nkwnd->is_dragging = 1;
                nkwnd->drag_offset.x = LOWORD(lParam);
                nkwnd->drag_offset.y = HIWORD(lParam);
            };            
        }
        break;

    case WM_LBUTTONUP:
        nkwnd->is_dragging = 0;
        break;

    case WM_MOUSEMOVE:
        {
            if (nkwnd->is_dragging && !nkwnd->is_maximized)
            {
                POINT cursorPos;
                GetCursorPos(&cursorPos);
                
                cursorPos.x -= nkwnd->drag_offset.x;
                cursorPos.y -= nkwnd->drag_offset.y;
                
                ShowWindow(nkwnd->window_handle, SW_RESTORE);
                SetWindowPos(nkwnd->window_handle, NULL, cursorPos.x, cursorPos.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
        }
        break;

    case WM_DROPFILES:
		break;

	case WM_KEYUP:
		if (wParam == VK_F1)
			MakeWindowScreenshot(nkwnd->window_handle);
		break;

	default:
		break;
	};

    if (nkwnd->nk_gdi_ctx && nk_gdi_handle_event(nkwnd->nk_gdi_ctx, hwnd, msg, wParam, lParam))
        return 0;

	return DefWindowProc(hwnd, msg, wParam, lParam);
};


static LRESULT CALLBACK nkgdi_window_proc_setup(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        CREATESTRUCT* ptrCr = (CREATESTRUCT*)lParam;
        struct nkgdi_window* nkgdi_wnd = (struct nkgdi_window*)ptrCr->lpCreateParams;

        SetWindowLongPtr(wnd, GWLP_USERDATA, (LONG_PTR)nkgdi_wnd);
        SetWindowLongPtr(wnd, GWLP_WNDPROC, (LONG_PTR)&nkgdi_window_proc_run);

        return nkgdi_window_proc_run(wnd, msg, wParam, lParam);
    };

    return DefWindowProc(wnd, msg, wParam, lParam);
};


void nkgdi_window_init(nkgdi_window_theme theme)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);

    /**
     *  Init gdiplus library for loading png images into bitmap
     *  NOTE: if its fails it will just makes blanks images
     */
    Gdiplus::GdiplusStartupInput GdiPlusInput;
    Gdiplus::Status GdiPlusStatus = Gdiplus::GdiplusStartup(&s_gdipToken, &GdiPlusInput, nullptr);
    if (GdiPlusStatus != Gdiplus::Ok)
        OUTPUTLN("init gdi+ failed %" PRIu32, GetLastError());

    /**
     *  Init nuklear gdi wnd class
     */
    WNDCLASSEXW wcex;
    std::memset(&wcex, 0, sizeof(wcex));

    /**
     * Check if class already registered
     */
    if (GetClassInfoExW(hInstance, NK_GDI_WINDOW_CLS, &wcex))
        return;

    wcex.cbSize         = sizeof(WNDCLASSEXW);
    wcex.style          = CS_OWNDC | CS_DBLCLKS;
    wcex.lpfnWndProc    = &nkgdi_window_proc_setup;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = GetModuleHandle(NULL);
    wcex.hIcon          = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = NULL;
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = NK_GDI_WINDOW_CLS;
    wcex.hIconSm        = NULL;

    ATOM ret = RegisterClassExW(&wcex);
    ASSERT(ret != NULL, "%u", GetLastError());
    if (ret != NULL)
        s_bIsProcessClassOwner = true;

    s_theme = (theme == nkgdi_window_theme_sys) ? nk_detect_theme() : theme;
};


void nkgdi_window_shutdown()
{
    ASSERT(s_imgRefCnt == 0, "free all images before shutdown ui subsystem: %u", s_imgRefCnt.load());

    s_wndList.clear();

    if (s_bIsProcessClassOwner)
    {
        UnregisterClassW(NK_GDI_WINDOW_CLS, GetModuleHandle(NULL));
        s_bIsProcessClassOwner = false;
    };

    if (s_gdipToken)
    {
        Gdiplus::GdiplusShutdown(s_gdipToken);
        s_gdipToken = NULL;
    };
};


void nkgdi_window_loop()
{
	/* modeless loop */
    nkgdi_window_loop_common();
};


void nkgdi_window_locale_changed()
{
    /* text container should be changed at this moment so just get & redraw all text strings with new locale */
    for (auto& it : s_wndList)
        nkgdi_window_post_redraw(it.get());
};


/*DLLSHARED*/ bool nkgdi_window_show(
    const char*             title,
    int32                   w,
    int32                   h,
    int32                   x,
    int32                   y,
    const char*             fontName,
    int32                   fontHeight,
    uint32                  flags,
    cb_draw                 cb,
    struct nkgdi_window*    parent,
    void*                   param
)
{
    std::shared_ptr<nkgdi_window> wnd = std::make_shared<nkgdi_window>();
    if (!wnd)
        return false;

    nk_size_correction(&w, &h);

	DWORD style = WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
	DWORD exstyle = (parent ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW) | WS_EX_ACCEPTFILES;

    RECT rc;
    SetRect(&rc, 0, 0, w, h);
    AdjustWindowRectEx(&rc, style, FALSE, exstyle);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;

    wnd->window_handle = CreateWindowExW(
        exstyle,
        NK_GDI_WINDOW_CLS,
        L"",
        style | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,              
        w,
        h,
        (parent ? parent->window_handle : nullptr),
        NULL,
        GetModuleHandleW(NULL),
        wnd.get()
    );
    
	/* default values for auto centering window */
	if (x == -1 || y == -1)
		nkgdi_window_center(wnd.get());

    if (!wnd->window_handle)
        return false;

    wnd->window_dc = GetWindowDC(wnd->window_handle);
	wnd->gdi_font = nk_gdifont_create(fontName, fontHeight);
    wnd->nk_ctx = nk_gdi_init(&wnd->nk_gdi_ctx, wnd->gdi_font, wnd->window_dc, w, h);
    wnd->flagsPrivate = 0;
    wnd->is_open = true;
    wnd->width = w;
    wnd->height = h;
    wnd->flags = flags;
    wnd->title = title;

    s_wndList.push_back(wnd);

    nkgdi_window_push_draw_proc(wnd.get(), cb);
	nkgdi_window_set_title(wnd.get(), title);
    nkgdi_window_set_param(wnd.get(), param);    
    nkgdi_window_post_redraw(wnd.get());

    if (parent)
    {
		/* modal loop */
        EnableWindow(parent->window_handle, FALSE);
        nkgdi_window_loop_common(wnd.get());
        EnableWindow(parent->window_handle, TRUE);
        SetActiveWindow(parent->window_handle);
	};

	return true;
};


/*DLLSHARED*/ void nkgdi_window_push_draw_proc(struct nkgdi_window* wnd, cb_draw cb)
{
    /**
     *  push and redraw if window has layered attribute
     *  OR push initial draw proc just once if not
     */
    if ((wnd->flags & nkgdi_window_flag_layered) || wnd->drawStack.empty())
    {
        wnd->drawStack.push_back(cb);
        nkgdi_window_post_redraw(wnd);
    };
};


/*DLLSHARED*/  void nkgdi_window_set_draw_proc(struct nkgdi_window* wnd, cb_draw cb)
{
    if (!wnd->drawStack.empty())
        wnd->drawStack.pop_back();
    
    wnd->drawStack.push_back(cb);
    nkgdi_window_post_redraw(wnd);
};


/*DLLSHARED*/ uint32 nkgdi_window_get_flags(struct nkgdi_window* wnd)
{
    return wnd->flags;
};


/*DLLSHARED*/ void nkgdi_window_set_flags(struct nkgdi_window* wnd, uint32 flags)
{
    wnd->flags = flags;
};


/*DLLSHARED*/ void nkgdi_window_post_redraw(struct nkgdi_window* wnd)
{
    /* do not call this in the same state of draw proc for avoid recursive spam post redraw */
    InvalidateRect(wnd->window_handle, NULL, TRUE);
};


/*DLLSHARED*/ void nkgdi_window_set_title(struct nkgdi_window* wnd, const char* title)
{
	if (wnd->title == title)
		return;

    wnd->title = title;
    std::wstring wtitle = nku_utf8_to_wstr(title);
    SetWindowTextW(wnd->window_handle, wtitle.c_str());
};


/*DLLSHARED*/ void nkgdi_window_get_title(struct nkgdi_window* wnd, const char* title, int32* size)
{

};


/*DLLSHARED*/ void nkgdi_window_set_size(struct nkgdi_window* wnd, int32 w, int32 h)
{
    if (wnd->width == w && wnd->height == h)
        return;
    
    nk_size_correction(&w, &h);

    wnd->width = w;
    wnd->height = h;
    wnd->ws_override = 1;
};


/*DLLSHARED*/ void nkgdi_window_get_size(struct nkgdi_window* wnd, int32* w, int32* h)
{
    struct nk_vec2 size = nk_window_get_size(wnd->nk_ctx);
    *w = int32(size.x);
    *h = int32(size.y);
};


/*DLLSHARED*/ void nkgdi_window_set_min_size(struct nkgdi_window* wnd, int32 w, int32 h)
{
    wnd->nk_ctx->style.window.min_size = nk_vec2(float(w), float(h));
};


/*DLLSHARED*/ void nkgdi_window_send_msg(struct nkgdi_window* wnd, int32 id, intptr_t param)
{
    wnd->messages.push_back({ id , param });
};


/*DLLSHARED*/ bool nkgdi_window_recv_msg(struct nkgdi_window* wnd, int32* id, intptr_t* param)
{
    if (wnd->messages.empty())
        return false;
    
    auto it = wnd->messages.begin();
    *id = (*it).id;
    *param = (*it).param;
    wnd->messages.pop_front();
    return true;
};


/*DLLSHARED*/ void nkgdi_window_set_param(struct nkgdi_window* wnd, void* param)
{
    wnd->params[0] = param;
};


/*DLLSHARED*/ void nkgdi_window_get_param(struct nkgdi_window* wnd, void** param)
{
    *param = wnd->params[0];
};


/*DLLSHARED*/ HWND nkgdi_window_get_native(struct nkgdi_window* wnd)
{
    return wnd->window_handle;
};


/*DLLSHARED*/ void nkgdi_window_set_ico(struct nkgdi_window* wnd, struct nk_image* img)
{   
	if (wnd->ico_img == img)
		return;

    wnd->ico_img = img;
	if (!wnd->ico_img)
    {
        HICON hIco = LoadIcon(NULL, IDI_APPLICATION);
        ::SendMessage(wnd->window_handle, WM_SETICON, ICON_BIG, (LPARAM)hIco);
        ::SendMessage(wnd->window_handle, WM_SETICON, ICON_SMALL, (LPARAM)hIco);
		return;
	};

    HBITMAP hbmMask = ::CreateCompatibleBitmap(::GetDC(NULL), img->w, img->h);
    if (hbmMask)
    {
        ICONINFO ii = { 0 };
        ii.fIcon = TRUE;
        ii.hbmColor = HBITMAP(img->handle.ptr);
        ii.hbmMask = hbmMask;

        HICON hIcon = ::CreateIconIndirect(&ii);
        if (hIcon)
        {
            ::SendMessage(wnd->window_handle, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            ::SendMessage(wnd->window_handle, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            ::DestroyIcon(hIcon);
        };

        ::DeleteObject(hbmMask);
    };
};


/*DLLSHARED*/ bool nk_img_create(struct nk_image* img, const char* path)
{
    bool bResult = false;
    
    HOBJ hFile = FileOpen(path, "rb");
    if (hFile)
    {
        uint32 fsize = uint32(FileSize(hFile));

        std::vector<char> buffer;
        buffer.resize(std::size_t(fsize));

        uint32 bytes = FileRead(hFile, &buffer[0], buffer.size());
        ASSERT(bytes == fsize);

        IStream* Stream = SHCreateMemStream(PBYTE(&buffer[0]), fsize);
        if (Stream)
        {
            HBITMAP hbm;
            Gdiplus::Bitmap Bitmap(Stream);
            Bitmap.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hbm);

            img->w = Bitmap.GetWidth();
            img->h = Bitmap.GetHeight();
            img->region[0] = 0;
            img->region[1] = 0;
            img->region[2] = img->w;
            img->region[3] = img->h;
            img->handle.ptr = hbm;
            ++s_imgRefCnt;
            
            Stream->Release();
            bResult = (hbm != NULL);
        };

        FileClose(hFile);
    };

    return bResult;
};


/*DLLSHARED*/ bool nk_img_create_rc(struct nk_image* img, int32 rcid)
{
    return nk_img_create_mod_rc(img, GetModuleHandle(NULL), rcid);
};


/*DLLSHARED*/ bool nk_img_create_mod_rc(struct nk_image* img, HINSTANCE hMod, int32 rcid)
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

        IStream* Stream = SHCreateMemStream(PBYTE(&buffer[0]), fsize);
        if (Stream)
        {
            HBITMAP hbm;
            Gdiplus::Bitmap Bitmap(Stream);
            Bitmap.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hbm);

            img->w = Bitmap.GetWidth();
            img->h = Bitmap.GetHeight();
            img->region[0] = 0;
            img->region[1] = 0;
            img->region[2] = img->w;
            img->region[3] = img->h;
            img->handle.ptr = hbm;
            ++s_imgRefCnt;

            Stream->Release();
            bResult = (hbm != NULL);
        };

        FileClose(hFile);
    };

    return bResult;
};


/*DLLSHARED*/ bool nk_img_create_mem(struct nk_image* img, const void* buff, uint32 size)
{
    bool bResult = false;

    IStream* Stream = SHCreateMemStream(PBYTE(buff), size);
    if (Stream)
    {
        HBITMAP hbm;
        Gdiplus::Bitmap Bitmap(Stream);
        Bitmap.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hbm);

        img->w = Bitmap.GetWidth();
        img->h = Bitmap.GetHeight();
        img->region[0] = 0;
        img->region[1] = 0;
        img->region[2] = img->w;
        img->region[3] = img->h;
        img->handle.ptr = hbm;
        ++s_imgRefCnt;

        Stream->Release();
        bResult = (hbm != NULL);
    };
    
    return bResult;
};


/*DLLSHARED*/ void nk_img_destroy(struct nk_image* img)
{
    if (img && img->handle.id != 0)
    {
        ASSERT(s_imgRefCnt > 0);
        --s_imgRefCnt;
        
        HBITMAP hbm = (HBITMAP)img->handle.ptr;
        DeleteObject(hbm);
        std::memset(img, 0, sizeof(struct nk_image));
    };
};