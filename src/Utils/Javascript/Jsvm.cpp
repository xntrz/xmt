#include "Jsvm.hpp"

#include "duktape/duktape.h"


static void* JsvmMalloc(void* UserData, duk_size_t Size)
{
#ifdef _DEBUG	
    return MemAlloc(Size, __FILE__, __LINE__);
#else
    return MemAlloc(Size);
#endif
};


static void* JsvmRealloc(void* UserData, void* Ptr, duk_size_t Size)
{
#ifdef _DEBUG	
    return MemRealloc(Ptr, Size, __FILE__, __LINE__);
#else
    return MemRealloc(Ptr, Size);
#endif
};


static void JsvmFree(void* UserData, void* Ptr)
{
#ifdef _DEBUG	
    MemFree(Ptr, __FILE__, __LINE__);
#else
    MemFree(Ptr);
#endif
};


static void JsvmFatalHandler(void* UserData, const char* Msg)
{
    DbgFatal(Msg);
};


static duk_ret_t JsvmNativePrint(duk_context* DukCtx)
{
    duk_push_string(DukCtx, " ");
    duk_insert(DukCtx, 0);
    duk_join(DukCtx, duk_get_top(DukCtx) - 1);
    OUTPUTLN("%s", duk_to_string(DukCtx, -1));
    return 0;
};


static duk_context* JsvmHeapCreate(void)
{
    duk_context* heap = duk_create_heap(JsvmMalloc, JsvmRealloc, JsvmFree, nullptr, JsvmFatalHandler);
    ASSERT(heap);
    if(heap)
    {
        duk_push_c_function(heap, JsvmNativePrint, DUK_VARARGS);
        duk_put_global_string(heap, "print");
    };

    return heap;
};


static void JsvmHeapDestroy(duk_context* heap)
{
    ASSERT(heap);
    duk_destroy_heap(heap);
};


CJsvm::exception::exception(const std::string& msg)
: m_MyMsg(msg)
{
    ;
};


char const* CJsvm::exception::what(void) const
{
    return m_MyMsg.c_str();
};


/*static*/ void CJsvm::Initialize(void)
{
    ;
};


/*static*/ void CJsvm::Terminate(void)
{
    ;
};


static inline duk_context* ConvOpaque(void* Opaque)
{
    ASSERT(Opaque);
    if (!Opaque)
        throw CJsvm::exception("invalid context");
    
    return (duk_context*)Opaque;
};


CJsvm::CJsvm(void)
: m_opaque(nullptr)
{
    m_opaque = JsvmHeapCreate();
    ASSERT(m_opaque);
};


CJsvm::~CJsvm(void)
{
    if (m_opaque)
    {
        JsvmHeapDestroy(ConvOpaque(m_opaque));
        m_opaque = nullptr;        
    };
};


std::string CJsvm::Eval(const std::string& Line)
{
    bool ThrowFlag = false;

    duk_context* DukCtx = ConvOpaque(m_opaque);
    duk_int_t RetCode = duk_peval_string(DukCtx, Line.c_str());
    if (RetCode != 0)
    {
        duk_safe_to_stacktrace(DukCtx, -1);
        ThrowFlag = true;
    }
    else
    {
        duk_safe_to_string(DukCtx, -1);
    };
    
    std::string Result = duk_get_string(DukCtx, -1);
	duk_pop(DukCtx);
    if ((Result == "undefined") || ThrowFlag)
        throw exception(Result);

    return Result;
};