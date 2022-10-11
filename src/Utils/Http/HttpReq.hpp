#pragma once

#include "HttpResponse.hpp"

#include "Shared/Network/NetProxy.hpp"


class CHttpReq final : public CListNode<CHttpReq>
{
public:
    struct context;
    struct redirect;
    struct netevent;
    struct container;

    using CompleteCallback = std::function<void(CHttpReq& req, CHttpResponse& resp)>;
    using ErrorCallback = std::function<void(CHttpReq& req, int32 errcode)>;

    static void initialize(void);
    static void terminate(void);

    //
    //  Suspends subsystem
    //  All pending requests will be canceled and new requests will failed with error until "resume" call
    //
    static void suspend(void);

    //
    //  Resumes subsystem
    //
    static void resume(void);

    //
    //  Cancel all pending requests
    //
    static void cancel_all(void);

    //
    //  Converts errcode to string
    //
    static const char* errcode_to_str(int32 errcode);

public:
    CHttpReq(void);

    //
    //  awaits all threads leave user callbacks and close internal socket
    //  note: object used in user proc should be valid while destructing pending request
    //
    ~CHttpReq(void);

    //
    //  Cancel request
    //
    void cancel(void);

    //
    //  Send current request to a specified URL
    //
    bool send(const std::string& Url, uint32 ConnectTimeout = 0);

    //
    //  Sets callback that notifies about request completion
    //
    void on_complete(CompleteCallback cb);

    //
    //  Sets callback that notifies about request failure
    //
    void on_error(ErrorCallback cb);

    //
    //  Sets request that will be send to server when connected
    //  If request is not empty (length > 0) request will be sent and response will by readed
    //  If request is empty (length == 0) no request will be sent and disconnect will be invoke 
    //
    void set_request(const std::string& Request);

    //
    //  Sets timeouts for reading
    //
    void set_timeout(uint32 ms);

    //
    //  Sets proxy for socket layer
    //
    void set_proxy(NetProxy_t ProxyType, uint64 NetAddr, void* Parameter = nullptr, int32 ParameterLen = 0);

    //
    //  Checks for request completion
    //  Returns true if request completed and response may be readed, otherwise returns false
    //
    bool is_complete(void);

    //
    //  Returns response object that contains all data received from server
    //
    CHttpResponse& response(void);

    //
    //  Infinitely awaits for request completion or error
    //
    void wait(void);

    //
    //  Waits specified milliseconds for request completion or error
    //  On sucess: returns true if request completed or error occur
    //  On failure: returns false if timeout occur
    //
    bool wait(uint32 ms);

    //
    //  Sets flag that allows internal implementation to resolve redirect requests automatically
    //  By default disabled
    //
    void resolve_redirect(bool flag);

    //
    //  Returns endpoint host, useful to retrieve hostname when resolving redirect flag is up.
    //  For getting non empty result request should be completed (call is_complete() to check it out)
    //  otherwise returns empty string
    //
    std::string get_host(void);

    //
    //  Returns error code value if error occur while processing request
    //
    int32 get_errcode(void);

private:
    context& ctx(void);

private:
    context* m_pContext;
};