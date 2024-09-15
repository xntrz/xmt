#pragma once


class CTvlResult final
{
public:
    enum ERRTYPE
    {
        ERRTYPE_NONE = 0,
        ERRTYPE_STREAM_NOT_EXIST,
        ERRTYPE_STREAM_NOT_LIVE,
        ERRTYPE_STREAM_PROTECTED,
        ERRTYPE_CLIP_NOT_EXIST,
        ERRTYPE_VOD_NOT_EXIST,

        ERRTYPENUM,
    };

public:
    CTvlResult();
    void OnAttach();
    void OnDetach();
    void OnStart();
    void OnStop();

    inline void SetError(ERRTYPE errtype) { m_errtype = errtype; };
    inline ERRTYPE GetError() const { return m_errtype; };

    inline void AddViewerCount(int32 count) { m_nViewerCount += count; };
    inline int32 GetViewerCount() const { return m_nViewerCount; };

    inline void SetViewerCountReal(int32 count) { m_nViewerCountReal = count; };
    inline int32 GetViewerCountReal() const { return m_nViewerCountReal; };

private:
    std::atomic<int32> m_nViewerCount;
    std::atomic<int32> m_nViewerCountReal;
    std::atomic<ERRTYPE> m_errtype;
};


extern CTvlResult TvlResult;