#include "Time.hpp"

#include <sys/timeb.h>


struct Time_t
{
	bool QpsOk;
	double QpsScale;
};


static Time_t Time;


void TimeInitialize(void)
{
	LARGE_INTEGER Freq = {};
	if (QueryPerformanceFrequency(&Freq))
	{
		Time.QpsScale = (1000.0 / Freq.QuadPart);
		Time.QpsOk = true;
	};
};


void TimeTerminate(void)
{
	;
};


uint32 TimeCurrentTick(void)
{
	return uint32(GetTickCount());
};


uint32 TimeCurrentTickPrecise(void)
{
	if (Time.QpsOk)
	{
		LARGE_INTEGER Counter = { 0 };
		QueryPerformanceCounter(&Counter);
		return uint32(Counter.QuadPart * Time.QpsScale);
	}
	else
	{
		return TimeCurrentTick();
	};
};


uint32 TimeCurrentUnix32(void)
{
	return TimeCurrentUnix32Ex(nullptr);
};


uint32 TimeCurrentUnix32Ex(uint32* Ms)
{
	__timeb32 tm32 = {};
	_ftime32_s(&tm32);

	if (Ms)
		*Ms = uint32(tm32.millitm);

	return tm32.time;
};


uint64 TimeCurrentUnix64(void)
{
	return TimeCurrentUnix64Ex(nullptr);
};


uint64 TimeCurrentUnix64Ex(uint32* Ms)
{
	__timeb64 tm64 = {};
	_ftime64_s(&tm64);

	std::tm tm = {};
	localtime_s(&tm, &tm64.time);

	if (Ms)
		*Ms = uint32(tm64.millitm);

	return tm64.time;
};


void TimeCurrentLocal(uint32* Hour, uint32* Minute, uint32* Second, uint32* Ms)
{
	uint32 y, m, d;
	TimeCurrentLocalEx(Hour, Minute, Second, Ms, &y, &m, &d);	
};


void TimeCurrentLocalEx(uint32* Hour, uint32* Minute, uint32* Second, uint32* Ms, uint32* Year, uint32* Month, uint32* Day)
{
	__timeb64 tm64 = {};
	std::tm tm = {};
	_ftime64_s(&tm64);
	localtime_s(&tm, &tm64.time);

	*Hour 	= tm.tm_hour;
	*Minute = tm.tm_min;
	*Second = tm.tm_sec;
	*Ms 	= tm64.millitm;
	*Year 	= tm.tm_year + 1900;
	*Month 	= tm.tm_mon + 1;
	*Day 	= tm.tm_mday;
};


void TimeStampSlice(uint32 Timestamp, uint32* Hour, uint32* Minute, uint32* Second, uint32* Ms)
{
	*Hour 	= ((((Timestamp / 1000) / 60) / 60) % 60);
	*Minute = (((Timestamp / 1000) / 60) % 60);
	*Second = ((Timestamp / 1000) % 60);
	*Ms 	= (Timestamp % 1000);
};