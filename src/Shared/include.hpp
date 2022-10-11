#pragma once

#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <Shlwapi.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <commdlg.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <Uxtheme.h>
#include <shellapi.h>
#include <tchar.h>
#include <wchar.h>
#include <ShObjIdl.h>
#include <winerror.h>
#include <Richedit.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <cctype>
#include <vector>
#include <stack>
#include <deque>
#include <unordered_map>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <condition_variable>

#pragma comment(lib, "PSAPI.lib")
#pragma comment(lib, "COMCTL32.lib")
#pragma comment(lib, "UXTHEME.lib")
#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Shlwapi.lib")

#pragma warning(disable : 4200)
#pragma warning(disable : 4996)
#pragma warning(disable : 4595)

#ifdef _UNICODE
namespace std
{
	typedef wstring tstring;
};
#else
namespace std
{
	typedef string tstring;
};
#endif


#include "typedefs.hpp"
#include "debug.hpp"
#include "list.hpp"
#include "queue.hpp"
#include "memstd.hpp"
#include "module.hpp"

#include "Common/String.hpp"

#define WM_USERMSG (WM_USER + 100)

#define assert ASSERT

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#define REF(p)          			(p)

#define COUNT_OF(ptr)	            int32(sizeof(ptr) / sizeof(ptr[0]))

#define BIT(no)			            (uint32(1 << (no)))

#define BITSOF(var)		            (sizeof(var) << 3)

#define BIT_SET(bitfield, no)       (bitfield |= (1 << no))

#define BIT_CLEAR(bitfield, no)     (bitfield &= ~(1 << no))

#define BIT_TEST(bitfield, no)    	((bitfield >> no) & 1)

#define FLAG_SET(maskfield, flag)			\
	maskfield |= flag

#define FLAG_CLEAR(maskfield, flag)			\
	maskfield &= ~flag

#define IS_FLAG_SET(flagfield, flag)		\
	bool((flagfield & flag) == flag)

#define IS_FLAG_SET_ANY(flagfield, flag)	\
	bool((flagfield & (flag)) != 0)

#define FLAG_CHANGE(flagfield, flag, set)	\
do											\
{											\
if (set)									\
	flagfield |= (flag);					\
else										\
	flagfield &= (~flag);					\
}											\
while (0)

#define ENUM_TO_FLAG(T)																\
	inline T operator~ (T a) { return (T)~(int32)a; }								\
	inline T operator| (T a, T b) { return (T)((int32)a | (int32)b); }				\
	inline T operator& (T a, T b) { return (T)((int32)a & (int32)b); }				\
	inline T operator^ (T a, T b) { return (T)((int32)a ^ (int32)b); }				\
	inline T& operator|= (T& a, T b) { return (T&)((int32&)a |= (int32)b); }		\
	inline T& operator&= (T& a, T b) { return (T&)((int32&)a &= (int32)b); }		\
	inline T& operator^= (T& a, T b) { return (T&)((int32&)a ^= (int32)b); }

#define ENUM_TO_FLAG_CLASS(T)														\
	inline friend T operator~ (T a) { return (T)~(int32)a; }						\
	inline friend T operator| (T a, T b) { return (T)((int32)a | (int32)b); }		\
	inline friend T operator& (T a, T b) { return (T)((int32)a & (int32)b); }		\
	inline friend T operator^ (T a, T b) { return (T)((int32)a ^ (int32)b); }		\
	inline friend T& operator|= (T& a, T b) { return (T&)((int32&)a |= (int32)b); }	\
	inline friend T& operator&= (T& a, T b) { return (T&)((int32&)a &= (int32)b); }	\
	inline friend T& operator^= (T& a, T b) { return (T&)((int32&)a ^= (int32)b); }

#define SWAP2(val)					\
	(((val) >> 8) & 0x00FF) | 		\
	(((val) << 8) & 0xFF00)

#define SWAP4(val)					\
	(((val) >> 24) & 0x000000FF) |	\
	(((val) >> 8)  & 0x0000FF00) | 	\
	(((val) << 8)  & 0x00FF0000) | 	\
	(((val) << 24) & 0xFF000000)

#define SWAP8(val)							\
	(((val) >> 56) & 0x00000000000000FF) |	\
	(((val) >> 40) & 0x000000000000FF00) | 	\
	(((val) >> 24) & 0x0000000000FF0000) |	\
	(((val) >> 8)  & 0x00000000FF000000) | 	\
	(((val) << 8)  & 0x000000FF00000000) |	\
	(((val) << 24) & 0x0000FF0000000000) |	\
	(((val) << 40) & 0x00FF000000000000) |	\
	(((val) << 56) & 0xFF00000000000000)


#define ALIGN_CHECK(v, a) 		((uint32(v) & ((uint32(a) - 1u))) == 0u)
#define ALIGN_ADJUST(v, a) 		(uint32(a) - (uint32(v) & (uint32(a) - 1u)))
#define ALIGN_ROUND_DOWN(v, a) 	(uint32(v) & ~(uint32(a) - 1u))
#define ALIGN_ROUND_UP(v, a) 	((uint32(v) + (uint32(a) - 1u)) & ~(uint32(a) - 1u))
#define ALIGN(v, a) 			(ALIGN_ROUND_UP(v, a))
#define Min(a, b) 				((a < b) ? (a) : (b))
#define Max(a, b) 				((a > b) ? (a) : (b))
#define Clamp(v, mi, ma)     	((v > ma) ? (ma) : ((v < mi) ? (mi) : (v)))
#define InvClamp(v, mi, ma)		((v > ma) ? (mi) : ((v < mi) ? (ma) : (v)))