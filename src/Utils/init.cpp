#include "init.hpp"
#include "module_obj.hpp"

#include "Json/Json.hpp"
#include "Http/HttpReq.hpp"
#include "Javascript/Jsvm.hpp"
#include "Html/HtmlObj.hpp"
#include "Websocket/Websocket.hpp"


void UtilsInitialize(void)
{
    module_obj_init();
    CHtmlObj::Initialize();
    CJsvm::Initialize();
    CJson::Initialize();;
};


void UtilsTerminate(void)
{
    /* if lib used in dll prevent it to unload or exit while any module obj exists */
    module_obj_wait_removes();
    
    CJson::Terminate();
    CJsvm::Terminate();
    CHtmlObj::Terminate();
    module_obj_term();
};