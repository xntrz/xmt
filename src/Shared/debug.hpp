#pragma once


#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)


DLLSHARED void DbgInitialize(void);
DLLSHARED void DbgTerminate(void);
DLLSHARED void DbgRegistModule(const char* pszLabel, HINSTANCE hInstance);
DLLSHARED void DbgRemoveModule(HINSTANCE hInstance);
DLLSHARED void DbgAssert(const char* expression, const char* fname, int32 fline);
DLLSHARED void DbgAssert(const char* expression, const char* fname, int32 fline, const char* format, ...);
DLLSHARED void DbgOutput(const char* fname, int32 fline, const char* format, ...);
DLLSHARED void DbgOutputLn(const char* fname, int32 fline, const char* format, ...);
DLLSHARED void DbgFatal(const char* reason, ...);


#ifdef ASSERT
#undef ASSERT
#endif

#ifdef _DEBUG

#define DBGBREAK            \
    do                      \
    {                       \
        __asm{ int 0x3 };   \
    } while (0)

#define ASSERT(expression, ...)                                             \
    do                                                                      \
    {                                                                       \
        if (!(expression))                                                  \
        {                                                                   \
	        DbgAssert(#expression, __FILE__, __LINE__, ##__VA_ARGS__);      \
        };                                                                  \
    } while(0)


#define OUTPUT(format, ...)     DbgOutput(__FILENAME__, __LINE__, format, ##__VA_ARGS__)
#define OUTPUTLN(format, ...)   DbgOutputLn(__FILENAME__, __LINE__, format, ##__VA_ARGS__)

#define ASSERT_EX ASSERT

#else

#define DBGBREAK

#define ASSERT(expression, ...) ((void)0)

#define OUTPUT(format, ...)     ((void)0)
#define OUTPUTLN(format, ...)   ((void)0)

#define ASSERT_EX(expression, ...) DbgFatal(#expression, ##__VA_ARGS__)

#endif