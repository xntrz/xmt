#pragma once

#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <CommCtrl.h>
#include <TlHelp32.h>
#include <tchar.h>
#include <Psapi.h>
#include <commdlg.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>

#include <algorithm>
#include <vector>
#include <stack>
#include <array>
#include <deque>
#include <unordered_map>
#include <functional>
#include <iterator>
#include <cinttypes>

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <type_traits>
#include <future>

#pragma comment(lib, "COMCTL32.lib")

/* structure or union contains an array that has zero size */
#pragma warning(disable : 4200)

/* 'function' : format string 'string' requires an argument of type 'type', but variadic argument number has type 'type' */
#pragma warning(disable : 4477)

#pragma warning(disable : 4595)
#pragma warning(disable : 4996)

#include "typedefs.hpp"
#include "debug.hpp"

#include "list.hpp"
#include "memstd.hpp"
#include "module.hpp"
#include "byteswap.hpp"
#include "clamp.hpp"
#include "string.hpp"

#define assert 				ASSERT
#define COUNT_OF(_array)	(_countof(_array))