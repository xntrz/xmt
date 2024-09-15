#include "TvlUI.hpp"
#include "TvlSettings.hpp"
#include "TvlResult.hpp"
#include "TvlObj.hpp"

#include "Shared/Common/Time.hpp"
#include "Shared/UI/nk_text.hpp"
#include "Shared/UI/nk_window.hpp"
#include "Shared/UI/nk_commdlg.hpp"
#include "Shared/UI/nk_utils.hpp"

#include "Utils/Proxy/ProxyGlue.hpp"
#include "Utils/Misc/WebUtils.hpp"


void TvlUI_DrawErrorMsgBox(CProxyGlue* glue, struct nkgdi_window* wnd, struct nk_context* ctx)
{
    if (TvlResult.GetError() == CTvlResult::ERRTYPE_NONE)
        return;

    /* NOTE: string order should match to CTvlResult::ERRTYPE enum */
    static const char* apszErrorMessages[] =
    {
        nullptr,
        nk_text_id(19),
        nk_text_id(20),
        nk_text_id(21),
        nk_text_id(22),
        nk_text_id(23),
    };

    static_assert(COUNT_OF(apszErrorMessages) == CTvlResult::ERRTYPENUM, "update me");

    CTvlResult::ERRTYPE err = TvlResult.GetError();

    ASSERT(err >= 0);
    ASSERT(err < COUNT_OF(apszErrorMessages));

    nk_msgbox(wnd, nk_msgbox_type_err, nk_msgbox_btn_ok, nk_text_id(24), apszErrorMessages[err]);

    TvlResult.SetError(CTvlResult::ERRTYPE_NONE);
};


void TvlUI_DrawBotParams(CProxyGlue* glue, struct nkgdi_window* wnd, struct nk_context* ctx, bool bDisabled)
{
	const float col_main [] = { 0.15f, 0.7f, 0.15f };
    nk_layout_row(ctx, NK_DYNAMIC, (21.0f * 6), COUNT_OF(col_main), col_main);
    nk_spacer(ctx);

    /* bot params */
    if (nk_group_begin(ctx, nk_text_id(2), NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(ctx, 5.0f, 1);
        nk_spacer(ctx);

        const float col_group [] = { 0.4f, 0.6f };
        nk_layout_row(ctx, NK_DYNAMIC, 20.0f, COUNT_OF(col_group), col_group);

        nk_flags textAlign = NK_TEXT_ALIGN_CENTERED | NK_TEXT_ALIGN_LEFT;

        /* target id */
        nk_label(ctx, nk_text_id(4), textAlign);
        {
            static char value[256] = { 0 };
            std::sprintf(value, "%s", TvlSettings.TargetId);

            nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX, value, COUNT_OF(value), nk_filter_default);

            std::strcpy(TvlSettings.TargetId, value);            
        }

        /* viewers */
        nk_label(ctx, nk_text_id(12), textAlign);
        {
            static char value[64] = { 0 };
            std::sprintf(value, "%" PRIu16, TvlSettings.Viewers);

            nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX, value, COUNT_OF(value), nk_filter_decimal);

            int port = std::atoi(value);
            port = clamp(port, 0, int(uint16_max));
            TvlSettings.Viewers = uint16(port);
        }

        /* keepalive mode flag */
        nk_label(ctx, nk_text_id(9), textAlign);
        {
            int KeepaliveMode = int(TvlSettings.RunMode);
            nk_checkbox_label(ctx, "", &KeepaliveMode);
            TvlSettings.RunMode = bool(KeepaliveMode);
        }

        nk_group_end(ctx);
    };

    nk_spacer(ctx);
};


void TvlUI_DrawBotResults(CProxyGlue* glue, struct nkgdi_window* wnd, struct nk_context* ctx)
{
    const float col_main [] = { 0.15f, 0.7f, 0.15f };
    nk_layout_row(ctx, NK_DYNAMIC, (21.0f * 6.0f), COUNT_OF(col_main), col_main);
    nk_spacer(ctx);

    /* check results */
    if (nk_group_begin(ctx, nk_text_id(10), NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(ctx, 5.0f, 1);
        nk_spacer(ctx);

        const float col_group [] = { 0.4f, 0.6f };
        nk_layout_row(ctx, NK_DYNAMIC, 20.0f, COUNT_OF(col_group), col_group);

        /* running time */
        nk_label(ctx, nk_text_id(11), NK_TEXT_LEFT);
        {
            uint32 elapsed = glue->GetElapsedTime();
            uint32 h = 0;
            uint32 m = 0;
            uint32 s = 0;
            uint32 ms = 0;
            TimeStampSlice(elapsed, &h, &m, &s, &ms);

            char value[256];
            std::sprintf(value, "%02" PRIu32 ":%02" PRIu32 ":%02" PRIu32, h, m, s);

            nk_label(ctx, value, NK_TEXT_LEFT);
        }

        /* viewers */
        nk_label(ctx, nk_text_id(12), NK_TEXT_LEFT);
        {
            char value[32];
            std::sprintf(value, "%" PRIuPTR, TvlResult.GetViewerCount());

            nk_label(ctx, value, NK_TEXT_LEFT);
        }

        /* botted */
        nk_label(ctx, nk_text_id(13), NK_TEXT_LEFT);
        {
            char value[32];
            std::sprintf(value, "%" PRIu32, TvlResult.GetViewerCountReal());

            nk_label(ctx, value, NK_TEXT_LEFT);
        }

        nk_group_end(ctx);
    };

    nk_spacer(ctx);
};


bool TvlUI_Draw(CProxyGlue* glue, struct nkgdi_window* wnd, struct nk_context* ctx, bool ret)
{
    nk_text_path_locale_push("tvl");
    nkgdi_window_set_title(wnd, nk_text_id(1));

    /* top blank space */
    nk_layout_row_dynamic(ctx, 16, 1);
    nk_spacer(ctx);

    /**
     * Draw check params
     * NOTE: its disabled for change while proxy system is running
     */
    bool bCheckParamsDisable = glue->IsRunning();
    if (bCheckParamsDisable)
        nk_widget_disable_begin(ctx);

    TvlUI_DrawBotParams(glue, wnd, ctx, bCheckParamsDisable);

    if (bCheckParamsDisable)
        nk_widget_disable_end(ctx);

    /* empty space between groups */
    nk_layout_row_dynamic(ctx, 8.0f, 1);
    nk_spacer(ctx);

    /* draw check results */
    TvlUI_DrawBotResults(glue, wnd, ctx);

    /* empty space */
    nk_layout_row_dynamic(ctx, 16.0f, 1);
    nk_spacer(ctx);

    /* start/stop button */
    const float col_btn [] = { 0.3f, 0.4f, 0.3f };
    nk_layout_row(ctx, NK_DYNAMIC, 28.0f, COUNT_OF(col_btn), col_btn);
    nk_spacer(ctx);

    if (glue->IsRunning())
    {
        if (nk_button_label(ctx, nk_text_id(15)))
            glue->Stop();
    }
    else
    {
        if (nk_button_label(ctx, nk_text_id(14)))
            glue->Start();
    };

    TvlUI_DrawErrorMsgBox(glue, wnd, ctx);

    nk_spacer(ctx);

    nk_text_path_pop();

    nku_adjust_window_size(wnd, ctx);

    return false;
};


bool TvlUI(struct nkgdi_window* wnd, struct nk_context* ctx, bool ret)
{
    if (!ret)
    {
        nkgdi_window_push_proxyglue(wnd, &TvlObjInitialize, &TvlObjTerminate, &TvlObjIsStopped, &TvlUI_Draw);
        nkgdi_window_post_redraw(wnd);
    };

    return ret;
};