#include "init.hpp"

#include "Json/Json.hpp"
#include "Http/HttpReq.hpp"
#include "Javascript/Jsvm.hpp"
#include "Html/HtmlObj.hpp"
#include "Websocket/Websocket.hpp"


void UtilsInitialize(void)
{
    CHtmlObj::Initialize();
    CJsvm::Initialize();
    CJson::Initialize();
    CHttpReq::initialize();
    CWebsocket::initialize();
};


void UtilsTerminate(void)
{
    CHttpReq::cancel_all();
    CWebsocket::abort_all();
    
    CWebsocket::terminate();
    CHttpReq::terminate();
    CJson::Terminate();
    CJsvm::Terminate();
    CHtmlObj::Terminate();
};