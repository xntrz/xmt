#pragma once


using int8      = int8_t;
using int16     = int16_t;
using int32     = int32_t;
using int64     = int64_t;

using uint8     = uint8_t;
using uint16    = uint16_t;
using uint32    = uint32_t;
using uint64    = uint64_t;

using HOBJ      = void*;

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

static const uint8 	uint8_min   = std::numeric_limits<uint8>::min();
static const uint8 	uint8_max   = std::numeric_limits<uint8>::max();
static const uint16 uint16_min  = std::numeric_limits<uint16>::min();
static const uint16 uint16_max  = std::numeric_limits<uint16>::max();
static const uint32 uint32_min  = std::numeric_limits<uint32>::min();
static const uint32 uint32_max  = std::numeric_limits<uint32>::max();
static const uint64 uint64_min  = std::numeric_limits<uint64>::min();
static const uint64 uint64_max  = std::numeric_limits<uint64>::max();
static const int8 	int8_min    = std::numeric_limits<int8>::min();
static const int8 	int8_max    = std::numeric_limits<int8>::max();
static const int16  int16_min   = std::numeric_limits<int16>::min();
static const int16  int16_max   = std::numeric_limits<int16>::max();
static const int32  int32_min   = std::numeric_limits<int32>::min();
static const int32  int32_max   = std::numeric_limits<int32>::max();
static const int64  int64_min   = std::numeric_limits<int64>::min();
static const int64  int64_max   = std::numeric_limits<int64>::max();