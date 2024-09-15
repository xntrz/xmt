#include "ProxyGlue.hpp"

#include "Shared/Common/Time.hpp"
#include "Shared/Common/Thread.hpp"
#include "Shared/UI/nk_window.hpp"


CProxyGlue::CProxyGlue(
    struct nkgdi_window*    wnd,
    cb_init_t 				cbInit,
    cb_term_t 				cbTerm,
    cb_stopcheck_t 			cbStopCheck,
    cb_draw_t 				cbDraw
)
: m_prevWndParam(nullptr)
, m_wnd(wnd)
, m_threadExitEvent()
, m_threadStopEvent()
, m_thread()
, m_timeStart(0)
, m_timeElapsed(0)
, m_cbInit(cbInit)
, m_cbTerm(cbTerm)
, m_cbStopCheck(cbStopCheck)
, m_cbDraw(cbDraw)
, m_cbGetClock(&TimeCurrentTickPrecise)
{
    ASSERT(m_cbInit != nullptr);
    ASSERT(m_cbTerm != nullptr);
    ASSERT(m_cbStopCheck != nullptr);
    ASSERT(m_cbDraw != nullptr);

    /* push proxy glue to wnd param */
    nkgdi_window_get_param(m_wnd, &m_prevWndParam);
    nkgdi_window_set_param(m_wnd, this);

    /* init & push cleanup proc */
    nkgdi_window_push_draw_proc(m_wnd, [](struct nkgdi_window* wnd, struct nk_context* ctx, bool ret) {
        if (ret)
        {
            CProxyGlue* glue = nullptr;
            nkgdi_window_get_param(wnd, reinterpret_cast<void**>(&glue));
            delete glue;
        };

        return ret;
    });

    /* init & push draw proc */
    nkgdi_window_push_draw_proc(m_wnd, [](struct nkgdi_window* wnd, struct nk_context* ctx, bool ret) {
        CProxyGlue* glue = nullptr;
        nkgdi_window_get_param(wnd, reinterpret_cast<void**>(&glue));
        return glue->m_cbDraw(glue, wnd, ctx, ret);
    });
};


CProxyGlue::~CProxyGlue()
{
    /* request force stop */
    Stop();

    /* return prev param */
    nkgdi_window_set_param(m_wnd, m_prevWndParam);
};


void CProxyGlue::Start()
{
    if (IsRunning())
        return;

    /* init per run data */
    m_timeStart = m_cbGetClock();
	m_timeElapsed = 0;
	
	/* init update check callback */
    m_threadExitEvent.reset();
    m_threadStopEvent.reset();
	m_thread = thread::spawn("ProxyGlue", [ this ]() {
		WndLayerLock(true);
		m_cbInit();
		while (!m_threadStopEvent.wait_for(std::chrono::seconds(1)))
        {
            if (m_cbStopCheck())
                break;

            m_timeElapsed = m_cbGetClock() - m_timeStart;
            nkgdi_window_post_redraw(m_wnd);
        };
        m_cbTerm();
        WndLayerLock(false);
        m_thread.detach();
        m_threadExitEvent.signal_once();
    });
};


void CProxyGlue::Stop()
{
    if (!IsRunning())
        return;

    m_threadStopEvent.signal_once();
    m_threadExitEvent.wait();
};


bool CProxyGlue::IsRunning() const
{
    return m_thread.joinable();
};


void CProxyGlue::WndLayerLock(bool bState)
{
    uint32 flags = nkgdi_window_get_flags(m_wnd);

    if (bState)
        flags |= (nkgdi_window_flag_disable_return);
    else
        flags &= ~(nkgdi_window_flag_disable_return);

    nkgdi_window_set_flags(m_wnd, flags);
};