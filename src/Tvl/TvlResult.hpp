#pragma once

#include "Utils/Proxy/ProxyResult.hpp"


class CTvlResult final : public CProxyResult
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
    static CTvlResult& Instance(void);

    virtual void Start(void) override;
    virtual void Stop(void) override;
    void AddViewerCountWork(int32 nViewerCount);
    int32 GetViewerCountWork(void);
    void SetViewerCountReal(int32 nViewerCount);
    int32 GetViewerCountReal(void);
    void SetError(ERRTYPE ErrorType);
    ERRTYPE GetError(void);
    void SetCtxObjWalkTime(uint32 WalkTimeMS);
    uint32 GetCtxObjWalkTime(void) const;

private:
    std::atomic<int32> m_nViewerCountWork;
    std::atomic<int32> m_nViewerCountReal;
    std::atomic<ERRTYPE> m_Errtype;
    uint32 m_aWalkTimestamps[8];
    int32 m_nWalkTimestampsNum;
    uint32 m_uWalktime;
};