#pragma once


template<class T, std::size_t N>
inline constexpr
std::size_t _fname_ofs(
    const T(&str)[N],
    std::size_t pos = N - 1u
)
{
    return (str[pos] == '\\') ? pos + 1u : (pos > 0u ? _fname_ofs(str, pos - 1u) : 0u);
};

template <class T, T v>
struct _fname_ofs_evaluate {
    static constexpr const T value = v;
};

#define __FILENAME__ (&__FILE__[_fname_ofs_evaluate<decltype(_fname_ofs(__FILE__)), _fname_ofs(__FILE__)>::value])


void DbgInitialize(void);
void DbgTerminate(void);
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
#define ASSERT(expression, ...)                                         \
    do {                                                                \
        if (!(expression)) {                                            \
	        DbgAssert(#expression, __FILE__, __LINE__, ##__VA_ARGS__);  \
        };                                                              \
    } while(0)
#define OUTPUT(format, ...)     DbgOutput(__FILENAME__, __LINE__, format, ##__VA_ARGS__)
#define OUTPUTLN(format, ...)   DbgOutputLn(__FILENAME__, __LINE__, format, ##__VA_ARGS__)
#define DBGBREAK()              (__debugbreak())
#else
#define ASSERT(expression, ...) ((void)0)
#define OUTPUT(format, ...)     ((void)0)
#define OUTPUTLN(format, ...)   ((void)0)
#define DBGBREAK()              ((void)0)
#endif