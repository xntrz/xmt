#pragma once

#include "Shared/Common/Event.hpp"


#define nkgdi_window_push_proxyglue(wnd, cbInit, cbTerm, cbCheckStop, cbDraw) \
	new CProxyGlue(wnd, cbInit, cbTerm, cbCheckStop, cbDraw);


/**
 *  Proxy system UI glue impl
 */
class CProxyGlue final
{
public:
    using cb_init_t = void(*)();
    using cb_term_t = void(*)();
    using cb_stopcheck_t = bool(*)();
    using cb_draw_t = bool(*)(CProxyGlue*, struct nkgdi_window*, struct nk_context*, bool);
    using cb_getlock_t = uint32(*)();

public:
    CProxyGlue(struct nkgdi_window* wnd, cb_init_t cbInit, cb_term_t cbTerm, cb_stopcheck_t cbStopCheck, cb_draw_t cbDraw);
    ~CProxyGlue();
    void Start();
    void Stop();
    bool IsRunning() const;
    void WndLayerLock(bool bState);

    inline uint32 GetElapsedTime() const { return m_timeElapsed; };

private:
    void* m_prevWndParam;
    struct nkgdi_window* m_wnd;
    CEvent m_threadExitEvent;
    CEvent m_threadStopEvent;
    std::thread m_thread;
    uint32 m_timeStart;
    uint32 m_timeElapsed;
    cb_init_t m_cbInit;
    cb_term_t m_cbTerm;
    cb_stopcheck_t m_cbStopCheck;
    cb_draw_t m_cbDraw;
    cb_getlock_t m_cbGetClock;
};